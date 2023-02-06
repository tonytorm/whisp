////
//  WhisperComponent.hpp
//  main
//
//  Created by Giulio Iacomino on 26/01/2023.
//
#pragma once



#include <thread>
#include <string>
#include <vector>


class WhisperComponent
{
public:
    // Terminal color map. 10 colors grouped in ranges [0.0, 0.1, ..., 0.9]
    // Lowest is red, middle is yellow, highest is green.
    const std::vector<std::string> k_colors = {
        "\033[38;5;196m", "\033[38;5;202m", "\033[38;5;208m", "\033[38;5;214m", "\033[38;5;220m",
        "\033[38;5;226m", "\033[38;5;190m", "\033[38;5;154m", "\033[38;5;118m", "\033[38;5;82m",
    };
    
    // helper function to replace substrings
    void replace_all(std::string & s, const std::string & search, const std::string & replace);
    
    // command-line parameters
    struct whisper_params {
        int32_t n_threads    = std::min(4, (int32_t) std::thread::hardware_concurrency());
        int32_t n_processors = 1;
        int32_t offset_t_ms  = 0;
        int32_t offset_n     = 0;
        int32_t duration_ms  = 0;
        int32_t max_context  = -1;
        int32_t max_len      = -1;
        int32_t best_of      = 5;
        int32_t beam_size    = -1;
        
        float word_thold    = 0.01f;
        float entropy_thold = 2.4f;
        float logprob_thold = -1.0f;
        
        bool speed_up       = false;
        bool translate      = false;
        bool diarize        = false;
        bool output_txt     = false;
        bool output_vtt     = false;
        bool output_srt     = false;
        bool output_wts     = false;
        bool output_csv     = false;
        bool print_special  = false;
        bool print_colors   = false;
        bool print_progress = false;
        bool no_timestamps  = false;
        
        std::string language = "en";
        std::string prompt;
        std::string model    = "models/ggml-base.en.bin";
        
        std::vector<std::string> fname_inp = {};
        std::vector<std::string> fname_outp = {};
    };
    struct whisper_print_user_data {
        const whisper_params * params;
        
        const std::vector<std::vector<float>> * pcmf32s;
    };
    
    void whisper_print_usage(int argc, char ** argv, const whisper_params & params);
    bool whisper_params_parse(int argc, char ** argv, whisper_params & params);
    
    //void whisper_print_segment_callback(struct whisper_context * ctx, int n_new, void * user_data);
    bool output_txt(struct whisper_context * ctx, const char * fname);
    bool output_vtt(struct whisper_context * ctx, const char * fname);
    bool output_srt(struct whisper_context * ctx, const char * fname, const whisper_params & params);
    bool output_csv(struct whisper_context * ctx, const char * fname);
    
    int runTranscription(whisper_context* ctx, whisper_params params);
    int callWhisperFullWithoutAudiofile(whisper_context* ctx, whisper_params params, const float* samples, int data_size, int processors);
};

