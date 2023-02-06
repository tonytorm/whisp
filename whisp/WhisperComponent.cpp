//
//  WhisperComponent.cpp
//  Whisp_playground
//
//  Created by Giulio Iacomino on 02/02/2023.
//  Copyright © 2023 Giulio Iacomino. All rights reserved.
//
#include <chrono>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <cmath>
#include <cstdio>
#include "WhisperComponent.hpp"
#include "whisper.h"
#define DR_WAV_IMPLEMENTATION
#include "../externalHeaders/dr_wav.h"

std::string to_timestamp(int64_t t, bool comma = false) {
    int64_t msec = t * 10;
    int64_t hr = msec / (1000 * 60 * 60);
    msec = msec - hr * (1000 * 60 * 60);
    int64_t min = msec / (1000 * 60);
    msec = msec - min * (1000 * 60);
    int64_t sec = msec / 1000;
    msec = msec - sec * 1000;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s%03d", (int) hr, (int) min, (int) sec, comma ? "," : ".", (int) msec);
    
    return std::string(buf);
}

int timestamp_to_sample(int64_t t, int n_samples) {
    return std::max(0, std::min((int) n_samples - 1, (int) ((t*WHISPER_SAMPLE_RATE)/100)));
}

// helper function to replace substrings
void WhisperComponent::replace_all(std::string & s, const std::string & search, const std::string & replace) {
    for (size_t pos = 0; ; pos += replace.length()) {
        pos = s.find(search, pos);
        if (pos == std::string::npos) break;
        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}
bool WhisperComponent::whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg[0] != '-') {
            params.fname_inp.push_back(arg);
            continue;
        }
        
        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"    || arg == "--threads")        { params.n_threads      = std::stoi(argv[++i]); }
        else if (arg == "-p"    || arg == "--processors")     { params.n_processors   = std::stoi(argv[++i]); }
        else if (arg == "-ot"   || arg == "--offset-t")       { params.offset_t_ms    = std::stoi(argv[++i]); }
        else if (arg == "-on"   || arg == "--offset-n")       { params.offset_n       = std::stoi(argv[++i]); }
        else if (arg == "-d"    || arg == "--duration")       { params.duration_ms    = std::stoi(argv[++i]); }
        else if (arg == "-mc"   || arg == "--max-context")    { params.max_context    = std::stoi(argv[++i]); }
        else if (arg == "-ml"   || arg == "--max-len")        { params.max_len        = std::stoi(argv[++i]); }
        else if (arg == "-bo"   || arg == "--best-of")        { params.best_of        = std::stoi(argv[++i]); }
        else if (arg == "-bs"   || arg == "--beam-size")      { params.beam_size      = std::stoi(argv[++i]); }
        else if (arg == "-wt"   || arg == "--word-thold")     { params.word_thold     = std::stof(argv[++i]); }
        else if (arg == "-et"   || arg == "--entropy-thold")  { params.entropy_thold  = std::stof(argv[++i]); }
        else if (arg == "-lpt"  || arg == "--logprob-thold")  { params.logprob_thold  = std::stof(argv[++i]); }
        else if (arg == "-su"   || arg == "--speed-up")       { params.speed_up       = true; }
        else if (arg == "-tr"   || arg == "--translate")      { params.translate      = true; }
        else if (arg == "-di"   || arg == "--diarize")        { params.diarize        = true; }
        else if (arg == "-otxt" || arg == "--output-txt")     { params.output_txt     = true; }
        else if (arg == "-ovtt" || arg == "--output-vtt")     { params.output_vtt     = true; }
        else if (arg == "-osrt" || arg == "--output-srt")     { params.output_srt     = true; }
        else if (arg == "-owts" || arg == "--output-words")   { params.output_wts     = true; }
        else if (arg == "-ocsv" || arg == "--output-csv")     { params.output_csv     = true; }
        else if (arg == "-of"   || arg == "--output-file")    { params.fname_outp.emplace_back(argv[++i]); }
        else if (arg == "-ps"   || arg == "--print-special")  { params.print_special  = true; }
        else if (arg == "-pc"   || arg == "--print-colors")   { params.print_colors   = true; }
        else if (arg == "-pp"   || arg == "--print-progress") { params.print_progress = true; }
        else if (arg == "-nt"   || arg == "--no-timestamps")  { params.no_timestamps  = true; }
        else if (arg == "-l"    || arg == "--language")       { params.language       = argv[++i]; }
        else if (                  arg == "--prompt")         { params.prompt         = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")          { params.model          = argv[++i]; }
        else if (arg == "-f"    || arg == "--file")           { params.fname_inp.emplace_back(argv[++i]); }
        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }
    
    return true;
}

void WhisperComponent::whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options] file0.wav file1.wav ...\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,        --help              [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,      --threads N         [%-7d] number of threads to use during computation\n",    params.n_threads);
    fprintf(stderr, "  -p N,      --processors N      [%-7d] number of processors to use during computation\n", params.n_processors);
    fprintf(stderr, "  -ot N,     --offset-t N        [%-7d] time offset in milliseconds\n",                    params.offset_t_ms);
    fprintf(stderr, "  -on N,     --offset-n N        [%-7d] segment index offset\n",                           params.offset_n);
    fprintf(stderr, "  -d  N,     --duration N        [%-7d] duration of audio to process in milliseconds\n",   params.duration_ms);
    fprintf(stderr, "  -mc N,     --max-context N     [%-7d] maximum number of text context tokens to store\n", params.max_context);
    fprintf(stderr, "  -ml N,     --max-len N         [%-7d] maximum segment length in characters\n",           params.max_len);
    fprintf(stderr, "  -bo N,     --best-of N         [%-7d] number of best candidates to keep\n",              params.best_of);
    fprintf(stderr, "  -bs N,     --beam-size N       [%-7d] beam size for beam search\n",                      params.beam_size);
    fprintf(stderr, "  -wt N,     --word-thold N      [%-7.2f] word timestamp probability threshold\n",         params.word_thold);
    fprintf(stderr, "  -et N,     --entropy-thold N   [%-7.2f] entropy threshold for decoder fail\n",           params.entropy_thold);
    fprintf(stderr, "  -lpt N,    --logprob-thold N   [%-7.2f] log probability threshold for decoder fail\n",   params.logprob_thold);
    fprintf(stderr, "  -su,       --speed-up          [%-7s] speed up audio by x2 (reduced accuracy)\n",        params.speed_up ? "true" : "false");
    fprintf(stderr, "  -tr,       --translate         [%-7s] translate from source language to english\n",      params.translate ? "true" : "false");
    fprintf(stderr, "  -di,       --diarize           [%-7s] stereo audio diarization\n",                       params.diarize ? "true" : "false");
    fprintf(stderr, "  -otxt,     --output-txt        [%-7s] output result in a text file\n",                   params.output_txt ? "true" : "false");
    fprintf(stderr, "  -ovtt,     --output-vtt        [%-7s] output result in a vtt file\n",                    params.output_vtt ? "true" : "false");
    fprintf(stderr, "  -osrt,     --output-srt        [%-7s] output result in a srt file\n",                    params.output_srt ? "true" : "false");
    fprintf(stderr, "  -owts,     --output-words      [%-7s] output script for generating karaoke video\n",     params.output_wts ? "true" : "false");
    fprintf(stderr, "  -ocsv,     --output-csv        [%-7s] output result in a CSV file\n",                    params.output_csv ? "true" : "false");
    fprintf(stderr, "  -of FNAME, --output-file FNAME [%-7s] output file path (without file extension)\n",      "");
    fprintf(stderr, "  -ps,       --print-special     [%-7s] print special tokens\n",                           params.print_special ? "true" : "false");
    fprintf(stderr, "  -pc,       --print-colors      [%-7s] print colors\n",                                   params.print_colors ? "true" : "false");
    fprintf(stderr, "  -pp,       --print-progress    [%-7s] print progress\n",                                 params.print_progress ? "true" : "false");
    fprintf(stderr, "  -nt,       --no-timestamps     [%-7s] do not print timestamps\n",                        params.no_timestamps ? "false" : "true");
    fprintf(stderr, "  -l LANG,   --language LANG     [%-7s] spoken language ('auto' for auto-detect)\n",       params.language.c_str());
    fprintf(stderr, "             --prompt PROMPT     [%-7s] initial prompt\n",                                 params.prompt.c_str());
    fprintf(stderr, "  -m FNAME,  --model FNAME       [%-7s] model path\n",                                     params.model.c_str());
    fprintf(stderr, "  -f FNAME,  --file FNAME        [%-7s] input WAV file path\n",                            "");
    fprintf(stderr, "\n");
}


void whisper_print_segment_callback(struct whisper_context * ctx, int n_new, void * user_data) {
    const auto & params  = *((WhisperComponent::whisper_print_user_data *) user_data)->params;
    const auto & pcmf32s = *((WhisperComponent::whisper_print_user_data *) user_data)->pcmf32s;
    
    const int n_segments = whisper_full_n_segments(ctx);
    
    std::string speaker = "";
    
    int64_t t0;
    int64_t t1;
    
    // print the last n_new segments
    const int s0 = n_segments - n_new;
    
    if (s0 == 0) {
        printf("\n");
    }
    
    for (int i = s0; i < n_segments; i++) {
        if (!params.no_timestamps || params.diarize) {
            t0 = whisper_full_get_segment_t0(ctx, i);
            t1 = whisper_full_get_segment_t1(ctx, i);
        }
        
        if (!params.no_timestamps) {
            printf("[%s --> %s]  ", to_timestamp(t0).c_str(), to_timestamp(t1).c_str());
        }
        
        if (params.diarize && pcmf32s.size() == 2) {
            const int64_t n_samples = pcmf32s[0].size();
            
            const int64_t is0 = timestamp_to_sample(t0, n_samples);
            const int64_t is1 = timestamp_to_sample(t1, n_samples);
            
            double energy0 = 0.0f;
            double energy1 = 0.0f;
            
            for (int64_t j = is0; j < is1; j++) {
                energy0 += fabs(pcmf32s[0][j]);
                energy1 += fabs(pcmf32s[1][j]);
            }
            
            if (energy0 > 1.1*energy1) {
                speaker = "(speaker 0)";
            } else if (energy1 > 1.1*energy0) {
                speaker = "(speaker 1)";
            } else {
                speaker = "(speaker ?)";
            }
            
            //printf("is0 = %lld, is1 = %lld, energy0 = %f, energy1 = %f, %s\n", is0, is1, energy0, energy1, speaker.c_str());
        }
        
        const char * text = whisper_full_get_segment_text(ctx, i);
        printf("%s%s", speaker.c_str(), text);
        
        // with timestamps or speakers: each segment on new line
        if (!params.no_timestamps || params.diarize) {
            printf("\n");
        }
        
        fflush(stdout);
    }
}

bool WhisperComponent::output_txt(struct whisper_context * ctx, const char * fname) {
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }
    
    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);
    
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        fout << text << "\n";
    }
    
    return true;
}

bool WhisperComponent::output_vtt(struct whisper_context * ctx, const char * fname) {
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }
    
    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);
    
    fout << "WEBVTT\n\n";
    
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        
        fout << to_timestamp(t0) << " --> " << to_timestamp(t1) << "\n";
        fout << text << "\n\n";
    }
    
    return true;
}

bool WhisperComponent::output_srt(struct whisper_context * ctx, const char * fname, const whisper_params & params) {
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }
    
    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);
    
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        
        fout << i + 1 + params.offset_n << "\n";
        fout << to_timestamp(t0, true) << " --> " << to_timestamp(t1, true) << "\n";
        fout << text << "\n\n";
    }
    
    return true;
}

bool WhisperComponent::output_csv(struct whisper_context * ctx, const char * fname) {
    std::ofstream fout(fname);
    if (!fout.is_open()) {
        fprintf(stderr, "%s: failed to open '%s' for writing\n", __func__, fname);
        return false;
    }
    
    fprintf(stderr, "%s: saving output to '%s'\n", __func__, fname);
    
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        if (text[0] == ' ') {
            text = text + sizeof(char); //whisper_full_get_segment_text() returns a string with leading space, point to the next character.
        }
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);
        
        //need to multiply times returned from whisper_full_get_segment_t{0,1}() by 10 to get milliseconds.
        fout << 10 * t0 << ", " << 10 * t1 << ", \"" << text    << "\"\n";
    }
    
    return true;
}

int WhisperComponent::runTranscription(whisper_context* ctx, whisper_params params){
    std::vector<whisper_token> prompt_tokens;
    
    // main loop here, running through each input filepath
    for (int f = 0; f < (int) params.fname_inp.size(); ++f) {
        fprintf(stderr, "new file\n");
        const auto fname_inp = params.fname_inp[f];
        const auto fname_outp = f < params.fname_outp.size() && !params.fname_outp[f].empty() ? params.fname_outp[f] : params.fname_inp[f];
        
        std::vector<float> pcmf32; // mono-channel F32 PCM
        std::vector<std::vector<float>> pcmf32s; // stereo-channel F32 PCM
        
        // WAV input
        {
            drwav wav;
            std::vector<uint8_t> wav_data; // used for pipe input from stdin
            
            if (fname_inp == "-") {
                {
                    uint8_t buf[1024];
                    while (true)
                    {
                        const size_t n = fread(buf, 1, sizeof(buf), stdin);
                        if (n == 0) {
                            break;
                        }
                        wav_data.insert(wav_data.end(), buf, buf + n);
                    }
                }
                
                if (drwav_init_memory(&wav, wav_data.data(), wav_data.size(), nullptr) == false) {
                    fprintf(stderr, "error: failed to open WAV file from stdin\n");
                    return 4;
                }
                
                fprintf(stderr, "%s: read %zu bytes from stdin\n", __func__, wav_data.size());
            }
            else if (drwav_init_file(&wav, fname_inp.c_str(), nullptr) == false) {
                fprintf(stderr, "error: failed to open '%s' as WAV file\n", fname_inp.c_str());
                return 5;
            }
            
            if (wav.channels != 1 && wav.channels != 2) {
                fprintf(stderr, "%s: WAV file '%s' must be mono or stereo\n", "", fname_inp.c_str());
                return 6;
            }
            
            if (params.diarize && wav.channels != 2 && params.no_timestamps == false) {
                fprintf(stderr, "%s: WAV file '%s' must be stereo for diarization and timestamps have to be enabled\n", "", fname_inp.c_str());
                return 6;
            }
            
            if (wav.sampleRate != WHISPER_SAMPLE_RATE) {
                fprintf(stderr, "%s: WAV file '%s' must be %i kHz\n", "", fname_inp.c_str(), WHISPER_SAMPLE_RATE/1000);
                return 8;
            }
            
            if (wav.bitsPerSample != 16) {
                fprintf(stderr, "%s: WAV file '%s' must be 16-bit\n", "", fname_inp.c_str());
                return 9;
            }
            
            
            const uint64_t n = wav_data.empty() ? wav.totalPCMFrameCount : wav_data.size()/(wav.channels*wav.bitsPerSample/8);
            
            std::vector<int16_t> pcm16;
            pcm16.resize(n*wav.channels);
            drwav_read_pcm_frames_s16(&wav, n, pcm16.data());
            drwav_uninit(&wav);
            
            // convert to mono, float
            pcmf32.resize(n);
            if (wav.channels == 1) {
                for (uint64_t i = 0; i < n; i++) {
                    pcmf32[i] = float(pcm16[i])/32768.0f;
                }
            } else {
                for (uint64_t i = 0; i < n; i++) {
                    pcmf32[i] = float(pcm16[2*i] + pcm16[2*i + 1])/65536.0f;
                }
            }
            
            if (params.diarize) {
                // convert to stereo, float
                pcmf32s.resize(2);
                
                pcmf32s[0].resize(n);
                pcmf32s[1].resize(n);
                for (uint64_t i = 0; i < n; i++) {
                    pcmf32s[0][i] = float(pcm16[2*i])/32768.0f;
                    pcmf32s[1][i] = float(pcm16[2*i + 1])/32768.0f;
                }
            }
        }
        // run the inference
        {
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            
            wparams.strategy = params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
            
            wparams.print_realtime   = false;
            wparams.print_progress   = params.print_progress;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.print_special    = params.print_special;
            wparams.translate        = params.translate;
            wparams.language         = params.language.c_str();
            wparams.n_threads        = params.n_threads;
            wparams.n_max_text_ctx   = params.max_context >= 0 ? params.max_context : wparams.n_max_text_ctx;
            wparams.offset_ms        = params.offset_t_ms;
            wparams.duration_ms      = params.duration_ms;
            
            wparams.token_timestamps = params.output_wts || params.max_len > 0;
            wparams.thold_pt         = params.word_thold;
            wparams.entropy_thold    = params.entropy_thold;
            wparams.logprob_thold    = params.logprob_thold;
            wparams.max_len          = params.output_wts && params.max_len == 0 ? 60 : params.max_len;
            
            wparams.speed_up         = params.speed_up;
            
            wparams.greedy.best_of        = (int)params.best_of;
            wparams.beam_search.beam_size = params.beam_size;
            
            wparams.prompt_tokens     = prompt_tokens.empty() ? nullptr : prompt_tokens.data();
            wparams.prompt_n_tokens   = prompt_tokens.empty() ? 0       : (int)prompt_tokens.size();
            
            whisper_print_user_data user_data = { &params, &pcmf32s };
            
            // this callback is called on each new segment
            if (!wparams.print_realtime) {
                wparams.new_segment_callback           = whisper_print_segment_callback;
                wparams.new_segment_callback_user_data = &user_data;
            }
            
            // example for abort mechanism
            // in this example, we do not abort the processing, but we could if the flag is set to true
            // the callback is called before every encoder run - if it returns false, the processing is aborted
            {
                static bool is_aborted = false; // NOTE: this should be atomic to avoid data race
                
                wparams.encoder_begin_callback = [](struct whisper_context * /*ctx*/, void * user_data) {
                    bool is_aborted = *(bool*)user_data;
                    return !is_aborted;
                };
                wparams.encoder_begin_callback_user_data = &is_aborted;
            }
            
            
            // buffers are fed here
            
            if (whisper_full_parallel(ctx, wparams, pcmf32.data(), (int)pcmf32.size(), params.n_processors) != 0) {
                fprintf(stderr, "%s: failed to process audio\n", "");
                return 10;
            }
        }
    }
    
    whisper_print_timings(ctx);
    whisper_free(ctx);
    return 0;
}

int WhisperComponent::callWhisperFullWithoutAudiofile(whisper_context* ctx, whisper_params params, const float* samples, int data_size, int processors){
    std::vector<std::vector<float>> pcmf32s;
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    wparams.strategy = params.beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    
    wparams.print_realtime   = false;
    wparams.print_progress   = params.print_progress;
    wparams.print_timestamps = !params.no_timestamps;
    wparams.print_special    = params.print_special;
    wparams.translate        = params.translate;
    wparams.language         = params.language.c_str();
    wparams.n_threads        = params.n_threads;
    wparams.n_max_text_ctx   = params.max_context >= 0 ? params.max_context : wparams.n_max_text_ctx;
    wparams.offset_ms        = params.offset_t_ms;
    wparams.duration_ms      = params.duration_ms;
    
    wparams.token_timestamps = params.output_wts || params.max_len > 0;
    wparams.thold_pt         = params.word_thold;
    wparams.entropy_thold    = params.entropy_thold;
    wparams.logprob_thold    = params.logprob_thold;
    wparams.max_len          = params.output_wts && params.max_len == 0 ? 60 : params.max_len;
    
    wparams.speed_up         = params.speed_up;
    
    wparams.greedy.best_of        = params.best_of;
    wparams.beam_search.beam_size = params.beam_size;
    
    wparams.prompt_tokens     = nullptr;
    wparams.prompt_n_tokens   = 0;
    
    whisper_print_user_data user_data = { &params, &pcmf32s };
    
    // this callback is called on each new segment
    if (!wparams.print_realtime) {
        wparams.new_segment_callback           = whisper_print_segment_callback;
        wparams.new_segment_callback_user_data = &user_data;
    }
    
    // example for abort mechanism
    // in this example, we do not abort the processing, but we could if the flag is set to true
    // the callback is called before every encoder run - if it returns false, the processing is aborted
    static bool is_aborted = false; // NOTE: this should be atomic to avoid data race
    
    wparams.encoder_begin_callback = [](struct whisper_context * /*ctx*/, void * user_data) {
        bool is_aborted = *(bool*)user_data;
        return !is_aborted;
    };
    wparams.encoder_begin_callback_user_data = &is_aborted;
    
    // buffers are fed here
    
    if (whisper_full_parallel(ctx, wparams, samples, data_size, params.n_processors != 0)) {
        fprintf(stderr, "%s: failed to process audio\n", "");
        return 10;
    }
    //whisper_print_timings(ctx);
    return 0;
}
