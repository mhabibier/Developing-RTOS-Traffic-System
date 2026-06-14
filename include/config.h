/**
 * @file config.h
 * @brief Konfigurasi global — Adaptive Smart Traffic Light with Emergency Vehicle Preemption
 *
 * ============================================================
 * REFERENSI LAPORAN:
 *   • Tabel 4.2  — Task Parameter Summary (periode, prioritas, stack, WCET)
 *   • Tabel 3.2  — Non-Functional Requirements (NFR)
 *     - NFR-01: Latensi ISR → task ≤ 5 ms
 *     - NFR-02: Emergency preemption end-to-end ≤ 100 ms
 *     - NFR-03: CPU utilization ≤ 70%
 *   • Section 4.2.3 — Justifikasi parameter scheduling
 *   • Section 4.4.3 — Priority Inheritance via mutex
 * ============================================================
 *
 * CATATAN KONVERSI STACK:
 *   xTaskCreate() menerima ukuran stack dalam WORD (bukan byte).
 *   Pada ESP32 (Xtensa LX6, 32-bit), 1 word = 4 byte.
 *   Contoh: STACK_EMERGENCY = 4096 → 4096 × 4 byte = 16 KB
 *   configMINIMAL_STACK_SIZE pada ESP32-Arduino = 2048 word (8 KB minimum).
 *   Stack task harus ≥ configMINIMAL_STACK_SIZE + kebutuhan lokal task.
 * ============================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ─────────────────────────────────────────────────────────────
 * 1. GPIO PIN MAPPING
 *    Lihat diagram.json untuk skematik lengkap.
 *    Konvensi: semua LED = OUTPUT, semua BTN = INPUT (+ pull-up)
 * ──────────────────────────────────────────────────────────── */

/* ── LED Jalur UTARA (North) ─────────────────────────────── */
#define PIN_LED_N_RED     4   ///< GPIO 4 → LED Merah Utara (safe, tidak ada fungsi khusus)
#define PIN_LED_N_YEL     5   ///< GPIO 5 → LED Kuning Utara (VSPICS0, aman post-boot)
#define PIN_LED_N_GRN    13   ///< GPIO 13 → LED Hijau Utara (MTCK, aman post-boot)

/* ── LED Jalur SELATAN (South) ───────────────────────────── */
#define PIN_LED_S_RED    14   ///< GPIO 14 → LED Merah Selatan (MTMS, aman post-boot)
#define PIN_LED_S_YEL    16   ///< GPIO 16 → LED Kuning Selatan
#define PIN_LED_S_GRN    17   ///< GPIO 17 → LED Hijau Selatan

/* ── LED Jalur TIMUR (East) ──────────────────────────────── */
#define PIN_LED_E_RED    18   ///< GPIO 18 → LED Merah Timur (VSPICLK)
#define PIN_LED_E_YEL    19   ///< GPIO 19 → LED Kuning Timur (VSPIQ)
#define PIN_LED_E_GRN    23   ///< GPIO 23 → LED Hijau Timur (VSPID)

/* ── LED Jalur BARAT (West) ──────────────────────────────── */
#define PIN_LED_W_RED    25   ///< GPIO 25 → LED Merah Barat (DAC1, aman sebagai GPIO)
#define PIN_LED_W_YEL    26   ///< GPIO 26 → LED Kuning Barat (DAC2)
#define PIN_LED_W_GRN    27   ///< GPIO 27 → LED Hijau Barat (MTDO, aman post-boot)

/* ── Tombol Kendaraan Normal (INPUT_PULLUP, interrupt FALLING) ─ */
#define PIN_BTN_N        32   ///< GPIO 32 → BTN_N (ADC1_CH4, full-featured, support pull-up)
#define PIN_BTN_S        33   ///< GPIO 33 → BTN_S (ADC1_CH5, support pull-up)
/**
 * @warning GPIO 12 & 15 adalah strapping pin.
 *   GPIO 12 (MTDI): level saat boot menentukan flash voltage.
 *     → Pada ESP32-DevKit-C, sudah ada resistor 10k pull-down ke GND.
 *     → Aman digunakan sebagai GPIO INPUT_PULLDOWN setelah boot.
 *     → Dalam Wokwi, pull-up internal tetap bisa digunakan (tidak ada
 *       risiko boot failure di simulator).
 *   GPIO 15 (MTDO): level tinggi = serial debug output aktif saat boot.
 *     → Aman untuk INPUT post-boot; jangan drive HIGH saat power-on.
 */
#define PIN_BTN_E        12   ///< GPIO 12 → BTN_E (*strapping pin, aman post-boot di Wokwi)
#define PIN_BTN_W        15   ///< GPIO 15 → BTN_W (*strapping pin, aman post-boot di Wokwi)

/* ── Tombol Kendaraan Darurat (INPUT-ONLY GPIO, pakai ext. pull-up 10kΩ) ─ */
/**
 * @note GPIO 34-39 adalah input-only (tidak ada internal pull-up/down).
 *   Diagram wokwi sudah menyertakan resistor pull-up 10kΩ ke 3.3V.
 *   Tombol menekan → pin ke GND → FALLING edge → trigger ISR.
 *   Gunakan mode FALLING (bukan RISING) untuk noise immunity.
 */
#define PIN_BTN_EMG_N    34   ///< GPIO 34 → BTN_EMG_N (input-only, ext. pull-up 10kΩ)
#define PIN_BTN_EMG_S    35   ///< GPIO 35 → BTN_EMG_S (input-only, ext. pull-up 10kΩ)
#define PIN_BTN_EMG_E    36   ///< GPIO 36 → BTN_EMG_E (VP, input-only, ext. pull-up 10kΩ)
#define PIN_BTN_EMG_W    39   ///< GPIO 39 → BTN_EMG_W (VN, input-only, ext. pull-up 10kΩ)

/* ── I2C LCD 16x2 ────────────────────────────────────────── */
#define PIN_I2C_SDA      21   ///< GPIO 21 → SDA (I2C default ESP32)
#define PIN_I2C_SCL      22   ///< GPIO 22 → SCL (I2C default ESP32)
#define LCD_I2C_ADDR   0x27   ///< Alamat I2C PCF8574 backpack (coba 0x3F jika tidak respon)
#define LCD_COLS         16
#define LCD_ROWS          2

/* ─────────────────────────────────────────────────────────────
 * 2. PERIODE TASK (dalam ms) — Tabel 4.2
 *    Semua task periodik menggunakan vTaskDelayUntil() untuk
 *    periodisitas presisi (tidak drift seperti vTaskDelay).
 * ──────────────────────────────────────────────────────────── */
#define PERIOD_EMERGENCY_MS      100  ///< T_emergency = 100 ms (sporadic, timeout polling)
#define PERIOD_CONTROLLER_MS      20  ///< T_controller = 20 ms (ref: 4.2.3 jitter ≤ 5%)
#define PERIOD_LIGHT_MS          100  ///< T_light = 100 ms (resolusi fase 100 ms)
#define PERIOD_MONITORING_MS     500  ///< T_monitoring = 500 ms (prioritas terendah)

/* ─────────────────────────────────────────────────────────────
 * 3. PRIORITAS TASK — Tabel 4.2
 *    Skala FreeRTOS: 0 = terendah, configMAX_PRIORITIES-1 = tertinggi
 *    ESP32-Arduino default configMAX_PRIORITIES = 25
 *    Prioritas 5 < prioritas IDF internal (level 10+), aman.
 * ──────────────────────────────────────────────────────────── */
#define PRIORITY_EMERGENCY    5   ///< Tertinggi — preempt semua task lain
#define PRIORITY_CONTROLLER   4   ///< Kalkulasi durasi fase
#define PRIORITY_LIGHT        3   ///< Aktuasi LED fisik
#define PRIORITY_MONITORING   2   ///< Terendah — laporan tidak kritis waktu

/* ─────────────────────────────────────────────────────────────
 * 4. UKURAN STACK TASK (dalam WORD) — Tabel 4.2
 *
 *    xTaskCreate(..., stackSizeInWords, ...)
 *    ESP32: 1 word = 4 byte (arsitektur 32-bit)
 *
 *    Konversi dari byte ke word:
 *      stackWords = stackBytes / 4
 *
 *    Tabel 4.2 (byte → word):
 *      Emergency   : 4096 byte  → 1024 word  (frame ISR + FreeRTOS overhead)
 *      Controller  : 4096 byte  → 1024 word  (kalkulasi proporsional)
 *      Light       : 3072 byte  →  768 word  (digitalWrite saja, minimal)
 *      Monitoring  : 6144 byte  → 1536 word  (Serial + LCD + buffer lokal)
 *
 *    configMINIMAL_STACK_SIZE di ESP32-Arduino = 2048 word → ≥ ini wajib.
 *    Namun xTaskCreatePinnedToCore minimum praktis ~1024 word (4 KB) untuk
 *    task sederhana. Nilai di bawah disesuaikan agar ≥ 1024.
 *
 *    @ref Section 4.6.2: Stack monitoring via uxTaskGetStackHighWaterMark()
 * ──────────────────────────────────────────────────────────── */
#define STACK_EMERGENCY    2048U  ///< 2048 word = 8 KB (emergency handler + mutex overhead)
#define STACK_CONTROLLER   2048U  ///< 2048 word = 8 KB (queue drain + kalkulasi)
#define STACK_LIGHT        2048U  ///< 2048 word = 8 KB (state machine + 12× digitalWrite)
#define STACK_MONITORING   3072U  ///< 3072 word = 12 KB (Serial printf + LCD + buffer)

/** Ambang batas stack warning: sisa < 20% dari ukuran awal → log ke Serial */
#define STACK_WARNING_THRESHOLD_PCT  20

/* ─────────────────────────────────────────────────────────────
 * 5. DEBOUNCE ISR
 *    NFR-01 (Tabel 3.2): Latensi ISR → task ≤ 5 ms
 *    Debounce 50 ms menolak glitch mekanik tombol tanpa mengorbankan
 *    responsivitas (kendaraan tidak menekan tombol dua kali < 50 ms).
 * ──────────────────────────────────────────────────────────── */
#define DEBOUNCE_MS   50UL   ///< 50 ms debounce per-pin (Tabel 3.2 NFR-01)

/* ─────────────────────────────────────────────────────────────
 * 6. PARAMETER FASE HIJAU — Algoritma Proporsional (FR-03)
 *    greenDuration = BASE_GREEN + (ratio_kendaraan × RANGE_GREEN)
 *    Hasil di-clamp ke [MIN_GREEN, MAX_GREEN]
 * ──────────────────────────────────────────────────────────── */
#define MIN_GREEN_MS    5000U   ///< Minimum durasi hijau: 5 detik
#define MAX_GREEN_MS   30000U   ///< Maksimum durasi hijau: 30 detik
#define BASE_GREEN_MS  10000U   ///< Durasi hijau dasar tanpa kendaraan: 10 detik
#define YELLOW_MS       3000U   ///< Durasi fase kuning: 3 detik (tetap)

/* ─────────────────────────────────────────────────────────────
 * 7. DURASI EMERGENCY HOLD
 *    Simulasi "kendaraan darurat melewati persimpangan"
 *    FR-02: Jalur darurat hijau selama EMERGENCY_HOLD_MS
 *    FR-07: Setelah selesai, kembali ke state sebelumnya
 * ──────────────────────────────────────────────────────────── */
#define EMERGENCY_HOLD_MS  5000U   ///< 5 detik jalur darurat diberi lampu hijau

/* ─────────────────────────────────────────────────────────────
 * 8. ENUMERASI
 * ──────────────────────────────────────────────────────────── */

/**
 * @enum Lane
 * @brief Identitas jalur persimpangan.
 *        Digunakan sebagai item Queue (4 byte / uint32_t).
 *        Urutan menentukan rotasi fase: N → E → S → W → N → ...
 *        (sesuai konvensi lalu-lintas persimpangan 4-arah)
 */
typedef enum {
    LANE_NORTH = 0,   ///< Jalur Utara
    LANE_EAST  = 1,   ///< Jalur Timur
    LANE_SOUTH = 2,   ///< Jalur Selatan
    LANE_WEST  = 3,   ///< Jalur Barat
    LANE_COUNT = 4    ///< Jumlah jalur (untuk iterasi array)
} Lane;

/**
 * @enum LightColor
 * @brief State warna lampu lalu lintas per jalur.
 */
typedef enum {
    LIGHT_RED    = 0,   ///< Lampu merah — jalur berhenti
    LIGHT_YELLOW = 1,   ///< Lampu kuning — fase transisi
    LIGHT_GREEN  = 2    ///< Lampu hijau — jalur berjalan
} LightColor;

/* ─────────────────────────────────────────────────────────────
 * 9. CORE AFFINITY (ref: Section 4.7 Multicore)
 * ──────────────────────────────────────────────────────────── */
#define CORE_APP   1   ///< Core 1 (APP_CPU): task real-time (Emergency, Controller, Light)
#define CORE_PRO   0   ///< Core 0 (PRO_CPU): task non-kritis (Monitoring, WiFi stack IDF)

#endif /* CONFIG_H */
