// SPDX-License-Identifier: MIT
// Newly authored demonstration melodies for this public project edition.
// Copyright (c) 2026 RT-Spark FreeRTOS Music Player contributors.
// See LICENSES/Demonstration-MIT.txt. These are not Arcade song transcriptions.
#ifndef BCA182_CUSTOM_SONGS_H
#define BCA182_CUSTOM_SONGS_H
#include <cstddef>
#include <cstdint>
namespace custom_songs {
struct Song {
    const char *display_name;
    const uint16_t *frequencies_hz;
    const uint16_t *durations_ms;
    const uint16_t *intervals_ms;
    std::size_t entry_count;
};
// Frequencies use rounded 12-tone equal temperament, A4 = 440 Hz.
// 0 Hz is a rest. Durations are milliseconds, independent of professor data.
// Each unit is 240 ms; sounded notes leave 35 ms of silence within the interval.
static constexpr uint16_t demo_0_frequencies_hz[] = {
    262, 392, 330, 440, 294, 0, 349, 494, 392, 330, 294, 0, 523, 349, 440, 294, 392, 0, 330, 494, 349, 294, 262, 0
};
static constexpr uint16_t demo_0_durations_ms[] = {
    445, 205, 205, 445, 205, 240, 205, 445, 205, 205, 445, 240, 445, 205, 205, 205, 445, 240, 205, 445, 205, 205, 685, 240
};
static constexpr uint16_t demo_0_intervals_ms[] = {
    480, 240, 240, 480, 240, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 240, 480, 240, 240, 480, 240, 240, 720, 240
};
static_assert(sizeof(demo_0_frequencies_hz) == sizeof(demo_0_durations_ms) && sizeof(demo_0_durations_ms) == sizeof(demo_0_intervals_ms), "Demo 0 array lengths");
static constexpr uint16_t demo_1_frequencies_hz[] = {
    220, 294, 349, 247, 330, 0, 440, 349, 294, 392, 247, 0, 330, 494, 392, 294, 349, 0, 440, 330, 247, 294, 220, 0
};
static constexpr uint16_t demo_1_durations_ms[] = {
    205, 445, 205, 205, 445, 240, 445, 205, 205, 445, 205, 240, 205, 445, 205, 205, 445, 240, 445, 205, 205, 205, 685, 240
};
static constexpr uint16_t demo_1_intervals_ms[] = {
    240, 480, 240, 240, 480, 240, 480, 240, 240, 480, 240, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 240, 720, 240
};
static_assert(sizeof(demo_1_frequencies_hz) == sizeof(demo_1_durations_ms) && sizeof(demo_1_durations_ms) == sizeof(demo_1_intervals_ms), "Demo 1 array lengths");
static constexpr uint16_t demo_2_frequencies_hz[] = {
    392, 587, 440, 523, 330, 0, 494, 392, 659, 440, 523, 0, 587, 349, 494, 659, 392, 0, 523, 440, 330, 494, 392, 0
};
static constexpr uint16_t demo_2_durations_ms[] = {
    445, 205, 445, 205, 205, 240, 205, 445, 205, 205, 445, 240, 445, 205, 205, 445, 205, 240, 205, 445, 205, 205, 685, 240
};
static constexpr uint16_t demo_2_intervals_ms[] = {
    480, 240, 480, 240, 240, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 480, 240, 240, 240, 480, 240, 240, 720, 240
};
static_assert(sizeof(demo_2_frequencies_hz) == sizeof(demo_2_durations_ms) && sizeof(demo_2_durations_ms) == sizeof(demo_2_intervals_ms), "Demo 2 array lengths");
static constexpr uint16_t demo_3_frequencies_hz[] = {
    262, 349, 294, 440, 330, 0, 392, 294, 494, 349, 262, 0, 330, 440, 349, 523, 392, 0, 294, 494, 330, 349, 262, 0
};
static constexpr uint16_t demo_3_durations_ms[] = {
    685, 205, 445, 205, 445, 240, 445, 205, 445, 205, 685, 240, 445, 205, 445, 205, 445, 240, 445, 205, 445, 205, 685, 240
};
static constexpr uint16_t demo_3_intervals_ms[] = {
    720, 240, 480, 240, 480, 240, 480, 240, 480, 240, 720, 240, 480, 240, 480, 240, 480, 240, 480, 240, 480, 240, 720, 240
};
static_assert(sizeof(demo_3_frequencies_hz) == sizeof(demo_3_durations_ms) && sizeof(demo_3_durations_ms) == sizeof(demo_3_intervals_ms), "Demo 3 array lengths");
static constexpr uint16_t demo_4_frequencies_hz[] = {
    294, 440, 349, 523, 392, 0, 330, 494, 440, 349, 587, 0, 392, 294, 494, 330, 440, 0, 523, 349, 392, 330, 294, 0
};
static constexpr uint16_t demo_4_durations_ms[] = {
    205, 205, 445, 205, 445, 240, 445, 205, 205, 445, 205, 240, 205, 445, 205, 445, 205, 240, 445, 205, 205, 445, 685, 240
};
static constexpr uint16_t demo_4_intervals_ms[] = {
    240, 240, 480, 240, 480, 240, 480, 240, 240, 480, 240, 240, 240, 480, 240, 480, 240, 240, 480, 240, 240, 480, 720, 240
};
static_assert(sizeof(demo_4_frequencies_hz) == sizeof(demo_4_durations_ms) && sizeof(demo_4_durations_ms) == sizeof(demo_4_intervals_ms), "Demo 4 array lengths");
static constexpr uint16_t demo_5_frequencies_hz[] = {
    220, 330, 262, 392, 294, 0, 349, 440, 262, 330, 494, 0, 392, 294, 349, 523, 330, 0, 440, 262, 392, 294, 220, 0
};
static constexpr uint16_t demo_5_durations_ms[] = {
    445, 445, 205, 205, 445, 240, 205, 445, 205, 445, 205, 240, 445, 205, 445, 205, 205, 240, 445, 205, 445, 205, 685, 240
};
static constexpr uint16_t demo_5_intervals_ms[] = {
    480, 480, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 480, 240, 480, 240, 240, 240, 480, 240, 480, 240, 720, 240
};
static_assert(sizeof(demo_5_frequencies_hz) == sizeof(demo_5_durations_ms) && sizeof(demo_5_durations_ms) == sizeof(demo_5_intervals_ms), "Demo 5 array lengths");
static constexpr uint16_t demo_6_frequencies_hz[] = {
    523, 392, 587, 440, 659, 0, 494, 349, 523, 392, 440, 0, 587, 494, 330, 523, 440, 0, 392, 659, 494, 349, 523, 0
};
static constexpr uint16_t demo_6_durations_ms[] = {
    205, 445, 205, 445, 205, 240, 445, 205, 445, 205, 205, 240, 205, 445, 205, 205, 445, 240, 445, 205, 205, 445, 685, 240
};
static constexpr uint16_t demo_6_intervals_ms[] = {
    240, 480, 240, 480, 240, 240, 480, 240, 480, 240, 240, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 480, 720, 240
};
static_assert(sizeof(demo_6_frequencies_hz) == sizeof(demo_6_durations_ms) && sizeof(demo_6_durations_ms) == sizeof(demo_6_intervals_ms), "Demo 6 array lengths");
static constexpr uint16_t demo_7_frequencies_hz[] = {
    294, 392, 330, 494, 349, 0, 440, 294, 523, 392, 330, 0, 494, 349, 440, 262, 392, 0, 330, 440, 349, 294, 262, 0
};
static constexpr uint16_t demo_7_durations_ms[] = {
    445, 205, 205, 445, 205, 240, 445, 205, 445, 205, 205, 240, 445, 205, 205, 445, 205, 240, 445, 205, 445, 205, 685, 480
};
static constexpr uint16_t demo_7_intervals_ms[] = {
    480, 240, 240, 480, 240, 240, 480, 240, 480, 240, 240, 240, 480, 240, 240, 480, 240, 240, 480, 240, 480, 240, 720, 480
};
static_assert(sizeof(demo_7_frequencies_hz) == sizeof(demo_7_durations_ms) && sizeof(demo_7_durations_ms) == sizeof(demo_7_intervals_ms), "Demo 7 array lengths");

static constexpr Song songs[] = {
    {"Pulse Path", demo_0_frequencies_hz, demo_0_durations_ms, demo_0_intervals_ms, sizeof(demo_0_frequencies_hz)/sizeof(demo_0_frequencies_hz[0])},
    {"Amber Steps", demo_1_frequencies_hz, demo_1_durations_ms, demo_1_intervals_ms, sizeof(demo_1_frequencies_hz)/sizeof(demo_1_frequencies_hz[0])},
    {"Blue Orbit", demo_2_frequencies_hz, demo_2_durations_ms, demo_2_intervals_ms, sizeof(demo_2_frequencies_hz)/sizeof(demo_2_frequencies_hz[0])},
    {"Quiet Grid", demo_3_frequencies_hz, demo_3_durations_ms, demo_3_intervals_ms, sizeof(demo_3_frequencies_hz)/sizeof(demo_3_frequencies_hz[0])},
    {"Copper Loop", demo_4_frequencies_hz, demo_4_durations_ms, demo_4_intervals_ms, sizeof(demo_4_frequencies_hz)/sizeof(demo_4_frequencies_hz[0])},
    {"Night Signal", demo_5_frequencies_hz, demo_5_durations_ms, demo_5_intervals_ms, sizeof(demo_5_frequencies_hz)/sizeof(demo_5_frequencies_hz[0])},
    {"Glass Ripple", demo_6_frequencies_hz, demo_6_durations_ms, demo_6_intervals_ms, sizeof(demo_6_frequencies_hz)/sizeof(demo_6_frequencies_hz[0])},
    {"Homeward Line", demo_7_frequencies_hz, demo_7_durations_ms, demo_7_intervals_ms, sizeof(demo_7_frequencies_hz)/sizeof(demo_7_frequencies_hz[0])},
};
static constexpr std::size_t song_count = sizeof(songs)/sizeof(songs[0]);
static_assert(song_count == 8, "Eight demonstration songs required");
} // namespace custom_songs
#endif
