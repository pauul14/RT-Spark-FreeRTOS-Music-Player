// Full custom-song player; functional baseline: verified volume checkpoint.
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "custom_songs.h"
#include "checkpoint_ui.h"
#include <cstring>
#include <cstdio>
#include <cmath>

static UART_HandleTypeDef uart;
static SemaphoreHandle_t lcd_mutex;
static SemaphoreHandle_t uart_mutex;
static TimerHandle_t confirmation_timer;
static TaskHandle_t button_task;
static TaskHandle_t ui_task;
static const auto &songs = custom_songs::songs;
static_assert(custom_songs::song_count == 8, "Three binary bits select eight songs");
static constexpr uint32_t timeout_event = 1U;
enum class PlayerState { STOPPED, PLAYING, PAUSED };
struct UiState {
    unsigned current;
    unsigned candidate;
    unsigned pending = 0; // Persistent short-release toggle mask, independent of playback.
    bool confirming;
    uint32_t timeout_sequence;
    PlayerState playback;
    unsigned volume_percent = 60;
    uint32_t volume_sequence = 0;
    unsigned current_entry = 0, total_entries = 0; // Read-only LCD progress snapshot.
    TickType_t selection_ui_started = 0; // Visual countdown only; timer logic unchanged.
    TickType_t entry_ui_deadline = 0, paused_ui_remaining = 0;
    unsigned progress_song = 0; // Read-only timing snapshot for elapsed-time display.
};
static UiState ui_state = {};
static unsigned requested_volume_index = 2; // 60%; protected by task critical sections.

// Only scalar state is copied while interrupts are masked; never draw/block here.
static void publish_ui(unsigned current, unsigned candidate, bool confirming, bool timed_out)
{
    taskENTER_CRITICAL();
    ui_state.current = current;
    ui_state.candidate = candidate;
    if (confirming && !ui_state.confirming) ui_state.selection_ui_started = xTaskGetTickCount();
    ui_state.confirming = confirming;
    if (timed_out) ++ui_state.timeout_sequence;
    taskEXIT_CRITICAL();
}

// Timer callback never blocks or accesses UART/state; polling owns selection state.
static void confirmation_expired(TimerHandle_t)
{
    xTaskNotify(button_task, timeout_event, eSetBits);
}

extern "C" void xPortSysTickHandler(void);
extern "C" void SysTick_Handler(void)
{
    // Both HAL and FreeRTOS use a 1 ms tick; no HAL_Delay inside tasks.
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}

static void uart_print_raw(const char *text)
{
    HAL_UART_Transmit(&uart, reinterpret_cast<uint8_t *>(const_cast<char *>(text)),
                      static_cast<uint16_t>(std::strlen(text)), 1000);
}

static void uart_print(const char *text)
{
    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE) {
        uart_print_raw(text);
        xSemaphoreGive(uart_mutex);
    }
}

static void stop(const char *reason)
{
    uart_print_raw(reason);
    __disable_irq();
    while (1) {}
}

extern "C" void vApplicationIdleHook(void)
{
    // Ordinary shallow sleep only: clocks and enabled wake-up interrupts remain on.
    // SysTick/TIM2 wake the core; no tick suppression or interrupt masking here.
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
    __ISB();
}

extern "C" void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    while (1) {}
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *)
{
    __disable_irq();
    while (1) {}
}

static void uart_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    uart.Instance = USART1;
    uart.Init.BaudRate = 115200;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&uart) != HAL_OK)
    {
        while (1)
        {
        }
    }
}

static TIM_HandleTypeDef buzzer_timer;
static uint32_t buzzer_timer_hz, buzzer_prescaler;
// adjust_volume owns this applied setting; TIM2 snapshots it at cycle boundaries.
static volatile uint32_t buzzer_duty_percent = 30;
static volatile bool waveform_active, waveform_high;
static volatile uint32_t waveform_period, waveform_high_ticks, waveform_low_ticks;

static uint32_t duty_high_ticks(uint32_t period, uint32_t duty)
{
    uint32_t high = (period*duty+50U)/100U;
    if (high<1) high=1;
    if (high>=period) high=period-1U;
    return high;
}
extern "C" void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&buzzer_timer); }
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer)
{
    if (timer->Instance != TIM2 || !waveform_active) return;
    if (!waveform_high) {
        // Begin a complete cycle using one coherent duty setting.
        waveform_high_ticks = duty_high_ticks(waveform_period, buzzer_duty_percent);
        waveform_low_ticks = waveform_period-waveform_high_ticks;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        __HAL_TIM_SET_AUTORELOAD(timer, waveform_high_ticks-1U);
        waveform_high = true;
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        __HAL_TIM_SET_AUTORELOAD(timer, waveform_low_ticks-1U);
        waveform_high = false;
    }
}

static void buzzer_silence(void)
{
    taskENTER_CRITICAL();
    waveform_active = false;
    HAL_TIM_Base_Stop_IT(&buzzer_timer);
    __HAL_TIM_CLEAR_FLAG(&buzzer_timer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    waveform_high = false;
    taskEXIT_CRITICAL();
}

static HAL_StatusTypeDef buzzer_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    __HAL_RCC_TIM2_CLK_ENABLE();
    RCC_ClkInitTypeDef clocks = {};
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    buzzer_timer_hz = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_HCLK_DIV1) buzzer_timer_hz *= 2U;
    const uint32_t division = (buzzer_timer_hz + 1000000U) / 2000000U;
    if (!division || division > 65536U) return HAL_ERROR;
    buzzer_prescaler = division - 1U;
    buzzer_timer.Instance = TIM2;
    buzzer_timer.Init.Prescaler = buzzer_prescaler;
    buzzer_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    buzzer_timer.Init.Period = 999;
    buzzer_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    buzzer_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&buzzer_timer) != HAL_OK) return HAL_ERROR;
    HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    return HAL_OK;
}

static HAL_StatusTypeDef buzzer_tone(float period_ms)
{
    buzzer_silence();
    // Preserve the same full-period quantization as the verified 50% engine.
    const float ticks = (static_cast<float>(buzzer_timer_hz)/(buzzer_prescaler+1U))*period_ms/2000.0f;
    if (!std::isfinite(ticks) || ticks<1.0f || ticks>10000000.0f) return HAL_ERROR;
    const uint32_t half_ticks = static_cast<uint32_t>(ticks+0.5f);
    taskENTER_CRITICAL();
    waveform_period = 2U*half_ticks;
    waveform_high_ticks = duty_high_ticks(waveform_period, buzzer_duty_percent);
    waveform_low_ticks = waveform_period-waveform_high_ticks;
    // Start LOW, then update interrupts alternate HIGH and LOW intervals.
    __HAL_TIM_SET_AUTORELOAD(&buzzer_timer, waveform_low_ticks-1U);
    __HAL_TIM_SET_COUNTER(&buzzer_timer, 0);
    HAL_StatusTypeDef status = HAL_TIM_GenerateEvent(&buzzer_timer, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_CLEAR_FLAG(&buzzer_timer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    if (status==HAL_OK) {
        waveform_active = true;
        status = HAL_TIM_Base_Start_IT(&buzzer_timer);
        if (status!=HAL_OK) waveform_active = false;
    }
    taskEXIT_CRITICAL();
    return status;
}

struct Playback {
    PlayerState state = PlayerState::STOPPED;
    unsigned song = 0, note = 0;
    TickType_t deadline = 0; // Full entry end, including source gap/rest.
    TickType_t tone_deadline = 0;
    TickType_t paused_remaining = 0, paused_tone_remaining = 0;
    uint16_t frequency_hz = 0;
    bool tone_active = false;
};
// Owned by polling_buttons; TIM2 ISR reads only independent waveform/duty state.
static Playback player;
static void publish_playback(PlayerState state)
{
    taskENTER_CRITICAL();
    ui_state.playback = state;
    ui_state.current_entry = player.note;
    ui_state.total_entries = static_cast<unsigned>(songs[player.song].entry_count);
    ui_state.entry_ui_deadline = player.deadline;
    ui_state.paused_ui_remaining = player.paused_remaining;
    ui_state.progress_song = player.song;
    taskEXIT_CRITICAL();
}
static bool reached(TickType_t now, TickType_t deadline)
{
    return static_cast<int32_t>(now-deadline) >= 0;
}
static TickType_t remaining_ticks(TickType_t now, TickType_t deadline)
{
    return reached(now,deadline) ? 0 : deadline-now;
}
static void playback_error(void)
{
    buzzer_silence();
    player.tone_active = false;
    player.state = PlayerState::STOPPED;
    publish_playback(player.state);
    uart_print("ERROR: custom song data/timer failure\r\n");
}
static bool playback_note(TickType_t started)
{
    const custom_songs::Song &song = songs[player.song];
    player.frequency_hz = song.frequencies_hz[player.note];
    const uint32_t duration_ms = song.durations_ms[player.note];
    const uint32_t interval_ms = song.intervals_ms[player.note];
    if (!duration_ms || interval_ms < duration_ms) { playback_error(); return false; }
    // Imported milliseconds directly; no professor beat/tempo scale is applied.
    player.deadline = started+pdMS_TO_TICKS(interval_ms);
    player.tone_deadline = started+pdMS_TO_TICKS(duration_ms);
    player.tone_active = player.frequency_hz != 0;
    if (!player.tone_active) buzzer_silence();
    else if (buzzer_tone(1000.0f/static_cast<float>(player.frequency_hz)) != HAL_OK) {
        playback_error(); return false;
    }
    publish_playback(player.state); // Publish progress only; audio deadlines unchanged.
    return true;
}
static void playback_start(unsigned index)
{
    buzzer_silence();
    player = Playback{}; // Every new confirmation restarts at entry zero.
    if (index >= custom_songs::song_count) { playback_error(); return; }
    player.song = index;
    const custom_songs::Song &song = songs[index];
    if (!song.entry_count || !song.frequencies_hz || !song.durations_ms || !song.intervals_ms) {
        playback_error(); return;
    }
    char line[128];
    std::snprintf(line,sizeof(line),"PLAY START | index=%u | name=\"%s\"\r\n",index,song.display_name);
    uart_print(line);
    player.state = PlayerState::PLAYING;
    publish_playback(player.state);
    playback_note(xTaskGetTickCount());
}
static void playback_poll(void)
{
    if (player.state != PlayerState::PLAYING) return;
    const TickType_t now = xTaskGetTickCount();
    if (player.tone_active && reached(now,player.tone_deadline)) {
        buzzer_silence();
        player.tone_active = false;
    }
    if (!reached(now,player.deadline)) return;
    // Preserve absolute entry boundaries to avoid cumulative poll rounding.
    const TickType_t next_started = player.deadline;
    buzzer_silence();
    player.tone_active = false;
    ++player.note;
    if (player.note >= songs[player.song].entry_count) {
        player.state = PlayerState::STOPPED;
        player.frequency_hz = 0;
        publish_playback(player.state);
        char line[64];
        std::snprintf(line,sizeof(line),"PLAY COMPLETE | index=%u\r\n",player.song);
        uart_print(line);
    } else playback_note(next_started);
}
static void playback_stop(void)
{
    if (player.state == PlayerState::STOPPED) return;
    buzzer_silence(); // Stop TIM2 and force PB0 LOW before publishing STOPPED.
    const unsigned confirmed_song = player.song;
    player = Playback{}; // Clear note, deadlines, saved pause time and progress.
    player.song = confirmed_song;
    publish_playback(player.state);
    char line[64];
    std::snprintf(line,sizeof(line),"PLAY STOPPED | index=%u\r\n",player.song);
    uart_print(line);
}

static void playback_toggle_pause(void)
{
    char line[96];
    if (player.state == PlayerState::PLAYING) {
        const TickType_t now = xTaskGetTickCount();
        player.paused_remaining = remaining_ticks(now,player.deadline);
        player.paused_tone_remaining = player.tone_active ? remaining_ticks(now,player.tone_deadline) : 0;
        buzzer_silence(); // TIM2 stopped; PB0 LOW even when pausing in a gap.
        player.tone_active = false;
        player.state = PlayerState::PAUSED;
        publish_playback(player.state);
        std::snprintf(line,sizeof(line),"PLAY PAUSED | index=%u | note=%u\r\n",player.song,player.note);
        uart_print(line);
    } else if (player.state == PlayerState::PAUSED) {
        const TickType_t now = xTaskGetTickCount();
        player.deadline = now+player.paused_remaining;
        player.tone_deadline = now+player.paused_tone_remaining;
        player.tone_active = player.paused_tone_remaining && player.frequency_hz;
        // Resume remaining tone only; a paused rest/gap remains silent.
        if (player.tone_active) {
            if (buzzer_tone(1000.0f/static_cast<float>(player.frequency_hz)) != HAL_OK) {
                playback_error(); return;
            }
        } else buzzer_silence();
        player.state = PlayerState::PLAYING;
        publish_playback(player.state);
        std::snprintf(line,sizeof(line),"PLAY RESUMED | index=%u | note=%u\r\n",player.song,player.note);
        uart_print(line);
    } else {
        // Replay the confirmed song after manual stop or natural completion.
        // Pending selection bits are deliberately unrelated to this song index.
        playback_start(player.song);
    }
}
static void lcd_song_names(unsigned index, char (&first)[20], char (&second)[20])
{
    // Existing LCD has two 19-character rows; wrap without driver changes.
    const char *name = songs[index].display_name;
    const std::size_t length = std::strlen(name);
    std::size_t split = length <= 19 ? length : 19;
    if (length > 19) {
        while (split && name[split] != ' ') --split;
        if (!split) split = 19;
    }
    std::memcpy(first,name,split);
    first[split] = '\0';
    name += split;
    while (*name == ' ') ++name;
    std::snprintf(second,sizeof(second),"%s",name);
}

static void update_lcd_leds_thread(void *)
{
    uint32_t song_totals_ms[8] = {};
    for (unsigned i=0;i<8;++i)
        for (std::size_t n=0;n<songs[i].entry_count;++n)
            song_totals_ms[i] += songs[i].intervals_ms[n];
    unsigned prefix_song=~0U, prefix_entry=~0U;
    uint32_t prefix_ms=0;
    int last_rgb = -1;
    uint32_t observed_timeout = 0;
    uint32_t observed_volume = 0;
    bool volume_visible = false;
    TickType_t volume_until = 0;
    bool timeout_visible = false;
    TickType_t timeout_until = 0;
    while (1) {
        UiState snapshot;
        taskENTER_CRITICAL();
        snapshot = ui_state;
        taskEXIT_CRITICAL();
        const TickType_t now = xTaskGetTickCount();
        if (snapshot.timeout_sequence != observed_timeout) {
            observed_timeout = snapshot.timeout_sequence;
            timeout_visible = true;
            timeout_until = now + pdMS_TO_TICKS(1500);
        }
        if (snapshot.volume_sequence != observed_volume) {
            observed_volume = snapshot.volume_sequence;
            volume_visible = true;
            volume_until = now+pdMS_TO_TICKS(1000);
        }
        if (snapshot.confirming || (volume_visible && (int32_t)(now-volume_until)>=0))
            volume_visible = false;
        // A new selection takes precedence over the transient timeout screen.
        if (snapshot.confirming || (timeout_visible && (int32_t)(now-timeout_until)>=0))
            timeout_visible = false;
        const unsigned mode = snapshot.confirming ? 1U : timeout_visible ? 2U : snapshot.playback==PlayerState::PLAYING ? 3U : snapshot.playback==PlayerState::PAUSED ? 4U : 0U;
        const unsigned index = snapshot.confirming ? snapshot.candidate : snapshot.current;
        const int rgb = snapshot.confirming ? 1 : snapshot.playback==PlayerState::PLAYING ? 3 : snapshot.playback==PlayerState::PAUSED ? 4 : 0;
        if (rgb != last_rgb) {
            // DMA completion wait blocks briefly; no LCD/state mutex is held.
            if (checkpoint_matrix_selection(rgb) == HAL_OK) last_rgb = rgb;
            else uart_print("ERROR: matrix refresh failed\r\n");
        }
        const unsigned volume_shown = volume_visible && mode!=1 && mode!=2 ? snapshot.volume_percent : 0;
        CheckpointLcdView view = {};
        view.mode = volume_shown ? 5U : mode;
        view.index = index;
        view.volume_percent = snapshot.volume_percent;
        view.pending_index = snapshot.pending;
        view.pending_name = songs[snapshot.pending].display_name;
        const unsigned entry=snapshot.progress_song==snapshot.current ? snapshot.current_entry : 0;
        const custom_songs::Song &current=songs[snapshot.current];
        if (prefix_song!=snapshot.current || prefix_entry!=entry) {
            prefix_ms=0;
            for (std::size_t n=0;n<entry && n<current.entry_count;++n)
                prefix_ms+=current.intervals_ms[n];
            prefix_song=snapshot.current;
            prefix_entry=entry;
        }
        view.completed = snapshot.playback==PlayerState::STOPPED && entry>=current.entry_count;
        view.total_ms=song_totals_ms[snapshot.current];
        view.elapsed_ms=prefix_ms;
        if (entry<current.entry_count && snapshot.progress_song==snapshot.current) {
            TickType_t remaining=0;
            if (snapshot.playback==PlayerState::PLAYING)
                remaining=remaining_ticks(now,snapshot.entry_ui_deadline);
            else if (snapshot.playback==PlayerState::PAUSED)
                remaining=snapshot.paused_ui_remaining;
            const uint32_t interval=current.intervals_ms[entry];
            uint32_t remaining_ms=remaining*portTICK_PERIOD_MS;
            if (remaining_ms>interval) remaining_ms=interval;
            if (snapshot.playback!=PlayerState::STOPPED) view.elapsed_ms+=interval-remaining_ms;
        }
        if (view.elapsed_ms>view.total_ms) view.elapsed_ms=view.total_ms;
        view.progress_permille=view.total_ms ? (view.elapsed_ms*1000UL)/view.total_ms : 0;
        view.animation_ms=now*portTICK_PERIOD_MS; // Decorative equalizer clock only.
        if (snapshot.confirming) {
            const TickType_t elapsed = now-snapshot.selection_ui_started;
            view.confirmation_remaining_ms = elapsed>=pdMS_TO_TICKS(5000) ? 0 :
                static_cast<unsigned>((pdMS_TO_TICKS(5000)-elapsed)*portTICK_PERIOD_MS);
        }
        // The renderer caches layout and updates only dirty regions for animations.
        if (xSemaphoreTake(lcd_mutex,pdMS_TO_TICKS(100))==pdTRUE) {
            char name1[20], name2[20];
            lcd_song_names(index,name1,name2);
            checkpoint_lcd_update(&view,name1,name2);
            xSemaphoreGive(lcd_mutex);
        } else uart_print("ERROR: LCD mutex wait timed out\r\n");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void buttons_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void song_message(const char *prefix, unsigned index)
{
    char message[256];
    std::snprintf(message, sizeof(message), "%s | index=%u | name=\"%s\"\r\n",
                  prefix,index,songs[index].display_name);
    uart_print(message);
}

static void polling_buttons(void *)
{
    enum class State { NORMAL, CONFIRMING };
    State state = State::NORMAL;
    unsigned selected_song = 0;
    unsigned candidate = 0;
    TickType_t confirmation_started = 0;
    const uint16_t pins[] = {GPIO_PIN_5, GPIO_PIN_1, GPIO_PIN_0, GPIO_PIN_4};
    struct DebouncedButton {
        bool raw;
        bool pressed;
        TickType_t changed;
    } buttons[4] = {};
    bool previous_up = false;
    TickType_t up_pressed_at = 0;
    bool up_consumed = false;
    bool up_began_confirming = false;
    unsigned press_bits = 0;
    unsigned pending_bits = 0;
    struct BitGesture {
        bool previous = false;
        bool active = false;
        bool blocked = false;
        bool volume_disqualified = false;
        bool consumed = false;
        TickType_t started = 0;
    } bit_gestures[3];
    uart_print("BCA182 CUSTOM SONG PLAYER\r\n");
    uart_print("Tap/release DOWN=toggle bit0; LEFT=toggle bit1; RIGHT=toggle bit2; bits persist\r\n");
    uart_print("Tap bits individually, then short UP=select; release and press UP within 5 seconds to confirm.\r\n");
    uart_print("Hold UP >=1500 ms: pause/resume, or replay the confirmed song when stopped.\r\n");
    uart_print("Hold DOWN >=1500 ms: stop and reset playback; long holds never toggle selection bits.\r\n");
    uart_print("UP/DOWN long gestures are disabled during the 5-second confirmation. Release before repeating.\r\n");
    uart_print("Volume: hold LEFT or RIGHT alone >=1200 ms, then release; do not press UP.\r\n");
    song_message("CURRENT SONG", selected_song);

    while (1) {
        const TickType_t now = xTaskGetTickCount();
        // One GPIO snapshot per poll, with independent 25 ms stable-level debounce.
        const uint32_t levels = GPIOC->IDR;
        for (unsigned i = 0; i < 4; ++i) {
            const bool pressed = (levels & pins[i]) == 0;
            if (pressed != buttons[i].raw) {
                buttons[i].raw = pressed;
                buttons[i].changed = now;
            }
            if ((TickType_t)(now - buttons[i].changed) >= pdMS_TO_TICKS(25))
                buttons[i].pressed = buttons[i].raw;
        }
        const bool up_edge = buttons[0].pressed && !previous_up;
        const bool up_release = !buttons[0].pressed && previous_up;
        previous_up = buttons[0].pressed;
        // Board-only volume-control adaptation; external potentiometer can replace this input later
        // Each bit gesture is classified on its debounced release. No held mask
        // is used for song selection; long LEFT/RIGHT can never toggle a bit.
        for (unsigned bit = 0; bit < 3; ++bit) {
            const unsigned button = bit+1;
            BitGesture &gesture = bit_gestures[bit];
            const bool edge = buttons[button].pressed && !gesture.previous;
            const bool release = !buttons[button].pressed && gesture.previous;
            gesture.previous = buttons[button].pressed;
            if (edge) {
                gesture.active = true;
                gesture.consumed = false;
                gesture.started = now;
                gesture.blocked = state==State::CONFIRMING || buttons[0].raw || buttons[0].pressed;
                gesture.volume_disqualified = gesture.blocked;
            }
            if (!gesture.active) continue;
            if (state==State::CONFIRMING || buttons[0].raw || buttons[0].pressed)
                gesture.blocked = true; // Latch through release, even across a timeout.
            for (unsigned other = 0; other < 4; ++other)
                if (other!=button && (buttons[other].raw || buttons[other].pressed))
                    gesture.volume_disqualified = true;
            const bool long_hold = (TickType_t)(now-gesture.started)>=
                pdMS_TO_TICKS(bit==0 ? 1500 : 1200);
            if (bit==0 && long_hold && !gesture.blocked && !gesture.consumed) {
                gesture.consumed = true; // One stop per hold, including while still held.
                playback_stop();
            }
            if (release) {
                if (!gesture.blocked && !gesture.consumed && !long_hold) {
                    pending_bits ^= 1U<<bit;
                    taskENTER_CRITICAL();
                    ui_state.pending = pending_bits;
                    taskEXIT_CRITICAL();
                    song_message("PENDING SONG",pending_bits);
                } else if (long_hold && bit!=0 && !gesture.blocked && !gesture.volume_disqualified) {
                    taskENTER_CRITICAL();
                    if (bit==1 && requested_volume_index>0) --requested_volume_index;
                    if (bit==2 && requested_volume_index<4) ++requested_volume_index;
                    taskEXIT_CRITICAL();
                }
                gesture.active = false;
            }
        }
        // RT-Spark board-specific USER_BUTTON equivalent: long press UP
        // In NORMAL, defer selection until release so a long hold cannot select.
        // Freeze the persistent pending mask on UP press; confirmation stays fixed.
        bool selection_event = false;
        if (up_edge) {
            up_pressed_at = now;
            press_bits = pending_bits;
            up_began_confirming = state==State::CONFIRMING;
            up_consumed = false;
            if (up_began_confirming) {
                selection_event = true; // Existing confirming press-edge behavior.
                up_consumed = true; // Cannot become pause after confirmation/timeout.
            }
        }
        if (!up_consumed && !up_began_confirming && state==State::NORMAL &&
            (buttons[0].pressed || up_release)) {
            const bool is_long = (TickType_t)(now-up_pressed_at)>=pdMS_TO_TICKS(1500);
            if (is_long) {
                up_consumed = true;
                playback_toggle_pause();
            } else if (up_release) {
                up_consumed = true;
                selection_event = true;
            }
        }
        uint32_t events = 0;
        xTaskNotifyWait(0, timeout_event, &events, 0);
        // The deadline check also rejects a press at/after expiry if processing is late.
        const bool deadline_reached = (TickType_t)(now - confirmation_started) >= pdMS_TO_TICKS(5000);
        if (state == State::CONFIRMING && ((events & timeout_event) || deadline_reached)) {
            configASSERT(xTimerStop(confirmation_timer, 0) == pdPASS);
            state = State::NORMAL;
            publish_ui(selected_song, candidate, false, true);
            uart_print("SELECTION TIMEOUT\r\n");
            song_message("CURRENT SONG", selected_song);
            // An edge on the expired deadline cannot confirm or start a new selection.
        } else if (selection_event) {
            if (state == State::NORMAL) {
                candidate = press_bits;
                confirmation_started = now;
                if (xTimerStart(confirmation_timer, 0) == pdPASS) {
                    state = State::CONFIRMING;
                    publish_ui(selected_song, candidate, true, false);
                    song_message("SONG CANDIDATE", candidate);
                    uart_print("Release UP, then press UP again within 5 seconds to confirm.\r\n");
                } else {
                    uart_print("ERROR: confirmation timer start failed\r\n");
                }
            } else {
                configASSERT(xTimerStop(confirmation_timer, 0) == pdPASS);
                selected_song = candidate;
                state = State::NORMAL;
                publish_ui(selected_song, candidate, false, false);
                song_message("SONG CONFIRMED", selected_song);
                playback_start(selected_song);
            }
        }
        playback_poll();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void adjust_volume(void *)
{
    unsigned last_applied = ~0U;
    while (1) {
        taskENTER_CRITICAL();
        const unsigned requested = requested_volume_index;
        taskEXIT_CRITICAL();
        if (requested != last_applied) {
            const unsigned user_percent = 20U*(requested+1U);
            const unsigned duty_percent = 10U*(requested+1U);
            taskENTER_CRITICAL();
            // Only this task applies volume. ISR adopts it on the next full cycle.
            buzzer_duty_percent = duty_percent;
            ui_state.volume_percent = user_percent;
            if (last_applied != ~0U) ++ui_state.volume_sequence;
            taskEXIT_CRITICAL();
            last_applied = requested;
            char message[80];
            std::snprintf(message,sizeof(message),"VOLUME | level=%u%% | duty=%u%%\r\n",
                          user_percent,duty_percent);
            uart_print(message);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main(void)
{
    HAL_Init();
    uart_init();
    uart_print_raw("BOOT: UART OK\r\n");
    buttons_init();
    if (buzzer_init()!=HAL_OK) stop("ERROR: TIM2 buzzer init failed\r\n");
    // Run HAL delays/DMA before ANY FreeRTOS object creation can mask SysTick.
    // No tasks exist here; task-context LCD drawing remains mutex protected.
    if (checkpoint_lcd_init() != HAL_OK) stop("ERROR: LCD init failed\r\n");
    if (checkpoint_matrix_init() != HAL_OK) stop("ERROR: matrix init failed\r\n");
    if (checkpoint_matrix_selection(0) != HAL_OK) stop("ERROR: matrix clear failed\r\n");

    lcd_mutex = xSemaphoreCreateMutex();
    uart_mutex = xSemaphoreCreateMutex();
    if (!lcd_mutex || !uart_mutex) stop("ERROR: mutex allocation failed\r\n");

    confirmation_timer = xTimerCreate("selection_timeout", pdMS_TO_TICKS(5000),
                                      pdFALSE, nullptr, confirmation_expired);
    if (!confirmation_timer) stop("ERROR: timer allocation failed\r\n");

    // Stack depths are 32-bit words; polling gets room for formatted names.
    if (xTaskCreate(update_lcd_leds_thread, "update_lcd_leds_thread", 512,
                    nullptr, 1, &ui_task) != pdPASS ||
        xTaskCreate(polling_buttons, "polling_buttons", 512,
                    nullptr, 1, &button_task) != pdPASS ||
        xTaskCreate(adjust_volume, "adjust_volume", 256,
                    nullptr, 1, nullptr) != pdPASS)
        stop("ERROR: task allocation failed\r\n");

    if (!ui_task) stop("ERROR: UI task handle is null\r\n");
    char task_report[128];
    std::snprintf(task_report, sizeof(task_report), "PLAYER READY | tasks=3 | free_heap=%lu\r\n",
                  (unsigned long)xPortGetFreeHeapSize());
    uart_print_raw(task_report);

    uart_print_raw("BOOT: STARTING SCHEDULER\r\n");
    vTaskStartScheduler();
    stop("ERROR: scheduler returned\r\n");
}
