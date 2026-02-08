#include <atomic>
#include "daisysp.h"
#include "../src/kxmx_bluemchen.h"
#include <string>
#include <stdio.h>

#define MAX_SIZE_PER_CHANNEL 2880000 

using namespace daisysp;
using namespace daisy;
using namespace kxmx;

Bluemchen bluemchen;

enum LooperState { IDLE, ARM_REC, RECORDING, PLAYING };

class QuantizedLooper {
public:
    float* buffer;
    LooperState state;
    int pos, loop_len, mod_len;
    bool is_triggered_internal;

    // グリッチ用変数
    int   slice_start;
    int   slice_len;
    int   slice_timer;

    void Init(float* buf_ptr) {
        buffer = buf_ptr;
        is_triggered_internal = false;
        slice_start = 0;
        slice_timer = 0;
        Reset();
    }

    void Reset() {
        state = IDLE;
        pos = 0; loop_len = 0;
        mod_len = MAX_SIZE_PER_CHANNEL;
        is_triggered_internal = false;
    }

    void Arm() { state = ARM_REC; }

    const char* GetStateStr() {
        switch (state) {
            case IDLE:      return "IDLE";
            case ARM_REC:   return "WAIT R";
            case RECORDING: return "REC...";
            case PLAYING:   return "LOOP";
            default:        return "ERR";
        }
    }

    float Process(float in, float cv_raw, float knob_val) {
        // --- 1. クロックエッジ検出 ---
        bool edge = false;
        if (!is_triggered_internal && cv_raw > 0.8f) {
            is_triggered_internal = true;
            edge = true;
        } else if (is_triggered_internal && cv_raw < 0.6f) {
            is_triggered_internal = false;
        }

        // --- 2. 状態遷移 & リトリガーロジック ---
        if (edge) {
            if (state == ARM_REC) {
                state = RECORDING; pos = 0; loop_len = 0;
            } else if (state == RECORDING) {
                state = PLAYING; mod_len = (pos > 0) ? pos : 1; pos = 0;
            } else if (state == PLAYING) {
                // 【新規】BPMクオンタイズ：クロックが来たら頭出し
                pos = 0;
                slice_timer = 0; // グリッチ周期もリセット
            }
        }

        // --- 3. オーディオ処理 ---
        if (state == IDLE || state == ARM_REC) return in;

        if (state == RECORDING) {
            if (pos < MAX_SIZE_PER_CHANNEL) {
                buffer[pos] = in;
                pos++;
            } else {
                state = PLAYING; mod_len = MAX_SIZE_PER_CHANNEL; pos = 0;
            }
            return in;
        }

        if (state == PLAYING) {
            float out_sample = 0.0f;

            // --- 4. オート・グリッチロジック ---
            if (knob_val < 0.05f) {
                // 通常再生
                out_sample = buffer[pos];
                pos++;
                if (pos >= mod_len) pos = 0;
            } else {
                // グリッチ再生 (2〜32分割)
                // ノブの値を分割数にマッピング (2, 4, 8, 16, 32のべき乗が音楽的)
                int divisions = 2;
                if (knob_val > 0.8f)      divisions = 32;
                else if (knob_val > 0.6f) divisions = 16;
                else if (knob_val > 0.4f) divisions = 8;
                else if (knob_val > 0.2f) divisions = 4;

                int current_slice_len = mod_len / divisions;
                if (current_slice_len < 1) current_slice_len = 1;

                // スライス時間を更新
                slice_timer++;
                if (slice_timer >= current_slice_len) {
                    slice_timer = 0;
                    // 次のスライスをランダムに選択
                    int num_slices = divisions;
                    slice_start = (rand() % num_slices) * current_slice_len;
                }

                int read_pos = slice_start + slice_timer;
                // バッファ境界チェック
                if (read_pos >= mod_len) read_pos = 0; 
                out_sample = buffer[read_pos];

                // 全体の再生位置(pos)も裏で回しておく（ノブを戻した時にズレないように）
                pos++;
                if (pos >= mod_len) pos = 0;
            }
            return out_sample;
        }
        return in;
    }
};

// --- システム共有変数 ---
float DSY_SDRAM_BSS buf_l[MAX_SIZE_PER_CHANNEL];
float DSY_SDRAM_BSS buf_r[MAX_SIZE_PER_CHANNEL];
QuantizedLooper looper_l, looper_r;

std::atomic<float> shared_cv_l{0.5f}, shared_cv_r{0.5f};
std::atomic<float> shared_knob_l{0.0f}, shared_knob_r{0.0f};

uint32_t last_oled_update = 0;
bool ignore_release = false;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    float cl = shared_cv_l.load(std::memory_order_relaxed);
    float cr = shared_cv_r.load(std::memory_order_relaxed);
    float kl = shared_knob_l.load(std::memory_order_relaxed);
    float kr = shared_knob_r.load(std::memory_order_relaxed);

    for(size_t i = 0; i < size; i++) {
        out[0][i] = looper_l.Process(in[0][i], cl, kl);
        out[1][i] = looper_r.Process(in[1][i], cr, kr);
    }
}

void ProcessControls() {
    bluemchen.ProcessAllControls();
    
    // CV入力
    shared_cv_l.store(bluemchen.controls[bluemchen.CTRL_3].Value(), std::memory_order_relaxed);
    shared_cv_r.store(bluemchen.controls[bluemchen.CTRL_4].Value(), std::memory_order_relaxed);
    
    // Knob入力 (CTRL_1, CTRL_2)
    shared_knob_l.store(bluemchen.controls[bluemchen.CTRL_1].Value(), std::memory_order_relaxed);
    shared_knob_r.store(bluemchen.controls[bluemchen.CTRL_2].Value(), std::memory_order_relaxed);

    if (bluemchen.encoder.TimeHeldMs() >= 1000) {
        looper_l.Reset(); looper_r.Reset();
        ignore_release = true;
    }
    if (bluemchen.encoder.FallingEdge()) {
        if (!ignore_release) { looper_l.Arm(); looper_r.Arm(); }
        ignore_release = false;
    }
}

int main(void) {
    bluemchen.Init();
    looper_l.Init(buf_l);
    looper_r.Init(buf_r);
    bluemchen.StartAdc();
    bluemchen.StartAudio(AudioCallback);

    while(1) {
        ProcessControls();
        uint32_t now = System::GetNow();
        if(now - last_oled_update > 30) {
            last_oled_update = now;
            bluemchen.display.Fill(false);
            bluemchen.display.DrawLine(31, 0, 31, 31, true);

            // --- 1. 状態表示 (WAIT / REC / LOOP) ---
            bluemchen.display.SetCursor(0, 0);
            bluemchen.display.WriteString(looper_l.GetStateStr(), Font_6x8, true);
            bluemchen.display.SetCursor(34, 0);
            bluemchen.display.WriteString(looper_r.GetStateStr(), Font_6x8, true);

            // --- 2. グリッチ分割数の表示 (LR独立表示) ---
            char gbuf[10];
            
            // L側のグリッチ状態表示
            float kl = shared_knob_l.load();
            if(kl < 0.05f) sprintf(gbuf, "NORM");
            else {
                int div = 2;
                if (kl > 0.8f) div = 32; else if (kl > 0.6f) div = 16;
                else if (kl > 0.4f) div = 8; else if (kl > 0.2f) div = 4;
                sprintf(gbuf, "/%d", div);
            }
            bluemchen.display.SetCursor(0, 24);
            bluemchen.display.WriteString(gbuf, Font_6x8, true);

            // R側のグリッチ状態表示 (ここを追加)
            float kr = shared_knob_r.load();
            if(kr < 0.05f) sprintf(gbuf, "NORM");
            else {
                int div = 2;
                if (kr > 0.8f) div = 32; else if (kr > 0.6f) div = 16;
                else if (kr > 0.4f) div = 8; else if (kr > 0.2f) div = 4;
                sprintf(gbuf, "/%d", div);
            }
            bluemchen.display.SetCursor(34, 24); // 右側に表示
            bluemchen.display.WriteString(gbuf, Font_6x8, true);

            // --- 3. プログレスバー ---
            // (前回同様のバー描画処理)
            if(looper_l.state == PLAYING || looper_l.state == RECORDING) {
                float p = (float)looper_l.pos / (looper_l.state == PLAYING ? looper_l.mod_len : MAX_SIZE_PER_CHANNEL);
                bluemchen.display.DrawRect(0, 12, (int)(p * 28.0f), 15, true, true);
            }
            if(looper_r.state == PLAYING || looper_r.state == RECORDING) {
                float p = (float)looper_r.pos / (looper_r.state == PLAYING ? looper_r.mod_len : MAX_SIZE_PER_CHANNEL);
                bluemchen.display.DrawRect(34, 12, 34 + (int)(p * 28.0f), 15, true, true);
            }

            bluemchen.display.Update();
        }
    }
}