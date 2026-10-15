#ifndef CHECKPOINT_UI_H
#define CHECKPOINT_UI_H
#include "stm32f4xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
HAL_StatusTypeDef checkpoint_lcd_init(void);
HAL_StatusTypeDef checkpoint_matrix_init(void);
HAL_StatusTypeDef checkpoint_matrix_selection(int mode);
// View modes: 0 stopped/ready, 1 selection, 2 cancelled, 3 playing,
// 4 paused, 5 temporary volume HUD. Called only with lcd_mutex held.
typedef struct {
    unsigned mode, index, volume_percent;
    unsigned progress_permille, confirmation_remaining_ms;
    unsigned completed;
    unsigned pending_index;
    const char *pending_name; // Static song string; display-only, no playback mutation.
    uint32_t elapsed_ms, total_ms, animation_ms;
} CheckpointLcdView;
void checkpoint_lcd_update(const CheckpointLcdView *view, const char *name1, const char *name2);
void checkpoint_lcd_volume(unsigned percent);
void checkpoint_lcd_render(unsigned mode, unsigned index, const char *name1, const char *name2);
#ifdef __cplusplus
}
#endif
#endif
