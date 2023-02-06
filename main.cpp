
#include <fstream>
#include <sstream>
#include <iostream>

#include "whisper.h"
#include "WhisperComponent.hpp"
#include "resamplingCode/CDSPResampler.h"
#include "gFunctions.hpp"

using namespace std;

int main(){
    atomic<bool> canRead(false);
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "STARTING MAIN THREAD\n" << '\n';
    
    std::thread whisperThread([start, &canRead]{
        // whisper init
        std::cout << "STARTING WHISPER THREAD\n" << '\n';
        WhisperComponent wComponent;
        WhisperComponent::whisper_params params;
        params.model = "/Users/tonytorm/Documents/gCoding/whisp/models/ggml-model-whisper-tiny.en.bin";
        whisper_context* ctx = whisper_init_from_file(params.model.c_str());
        while (true){
            // also check file is not being written to in this moment
            
            
            ifstream infile("/Users/tonytorm/Documents/gCoding/whisp/TESTFILES/MIXER-A-T01_split1.bin", std::ios::binary | std::ios::ate);
            if (infile && canRead.load()){
                int size = (int)infile.tellg();
                infile.seekg(0, std::ios::beg);
                int sizeInFloats = size/sizeof(float);
                float* data = new float[sizeInFloats];
                infile.read((char*)data, size);
                infile.close();
                wComponent.callWhisperFullWithoutAudiofile(ctx,
                                                           params,
                                                           data,
                                                           sizeInFloats,
                                                           params.n_processors != 0);
                delete [] data;
                std::cout << "DONE. TIME ELAPSED -  " << '\n';
                printElapsedTimeSince(start);
                break;
            }else{
                std::this_thread::sleep_for(std::chrono::seconds(5)); // check every 5 seconds
                std::cout << "FILE IS NOT READY, WAITING 5 SECONDS\n" << '\n';
            }
        }
    });
    
    
    
    
    const double OutSampleRate = 16000.0;
    AudioFile wavFile;
    std::string partialFilePath = "/Users/tonytorm/Desktop/kraken_media/bal/";
    std::string fileName = "MIXER-A-T01";
    std::string filePath = partialFilePath + fileName + ".WAV";
    if (readHeader(filePath, &wavFile)){
        std::cout << "Wave file parsed -  ";
        printElapsedTimeSince(start);
        ifstream inputStream;
        std::vector<ofstream> outputStreams;
        inputStream.open(filePath, ifstream::binary); //49152
        if(!inputStream.good()){
            cout << "error while trying to open buffer\n";
            return -1; // error
        }
        
        inputStream.seekg((uint32_t)wavFile._datalocation);

        
//        uint32_t dataChunkSize = (uint32_t)(wavFile._datasize/wavFile._ChannelCount) /3 * 2; // force to 16 bit depth
//        uint32_t fmtChunkSize = 16;
        //uint32_t totalFileSize = (8 + fmtChunkSize) + (8 + dataChunkSize);
        for (int i = 0; i < wavFile._ChannelCount; i++){
            std::string folderPath = "/Users/tonytorm/Documents/gCoding/whisp/TESTFILES/";
            std::string str = folderPath + fileName + "_split" + std::to_string(i) + ".bin";
            const char* outputFileName = str.c_str();
            outputStreams.emplace_back(ofstream(outputFileName, ofstream::binary));
            //ofstream& stream = outputStreams.back();
//            writeRIFFChunk(stream, totalFileSize);
//            writeFMTChunk(stream, OutSampleRate, 16, 1);    // write a 16bit mono file for each channel
//            writeDataChunkHeader(stream, dataChunkSize);
        }
        
        const int InBufCapacity = 1024;
        int channelCount = wavFile._ChannelCount;
        
        std::vector<std::unique_ptr<r8b::CDSPResampler24>> resamplers(channelCount);
        int bytesPerSample = wavFile._bitdepth/8;
        int frameSize = bytesPerSample*wavFile._ChannelCount;  // 1 sample for each channel
        const size_t INPUTBUFFERSIZE = frameSize * InBufCapacity;
        std::vector<std::vector<double>> convertedPlanarBuffers; // from raw data to double

        for (int i = 0; i < channelCount; i++){       // allocate a buffer for each channel (planar)
            convertedPlanarBuffers.push_back(std::vector<double>(1024));
            resamplers[i].reset(new r8b::CDSPResampler24(48000.0, OutSampleRate, InBufCapacity));
        }
        char buffer[INPUTBUFFERSIZE];                          // allocate an interleaved buffer
        int dataRead = 0;
        std::vector<std::vector<float>> floatBuffer(channelCount);
        //std::cout << "nb of required buffers: " << wavFile._datasize / INPUTBUFFERSIZE << '\n';
        while (inputStream.read(buffer, INPUTBUFFERSIZE)) { // buffer speed
            static int bufferCounter = 0;
            bufferCounter++;
            static int count = 0;
            if (count ==0){    // just signal start of processing
                std::cout << "Starting bit depth/sampling conversion -  ";
                printElapsedTimeSince(start);
                count++;
            }
            int b = 0;
            
            // should we zero the buffers as well?
            for (int i = 0; i < INPUTBUFFERSIZE; i+=frameSize){  // frame speed
                for (int j = 0; j < channelCount; j++){
                    char rawData[bytesPerSample];
                    for (int z = 0; z < bytesPerSample; z++){
                        rawData[z] = buffer[i + (j*bytesPerSample+z)];
                    }
                    double sample = convertByteArrayToDouble(rawData);
                    convertedPlanarBuffers.at(j).at(b) = sample;
                }
                b++;  // this counter is here to index samples
            }
            //buffer speed
            double* resampledBuffers[channelCount];
            for (int j = 0; j < channelCount; j++){
                auto& resampler = resamplers[j];
                int writeCount = 0;
                writeCount = resampler->process(convertedPlanarBuffers[j].data(), InBufCapacity, resampledBuffers[j]);
                
                
                int prevSize = (int)floatBuffer[j].size();
                floatBuffer[j].resize(prevSize + writeCount);
                for (int i = 0; i < writeCount; i++){
                    double* doubleBuffer = resampledBuffers[j];
                    floatBuffer[j][i+prevSize] = doubleBuffer[i];
                }
                
                
                
//                if (floatBuffer.size() != 0) std::cout << "FLOAT BUFFER SIZE :" << floatBuffer.size() << '\n';
//                for (int i = prevSize; i < floatBuffer.size(); i++){
//                    floatBuffer[i] = -1;
//                }
//                    char outSample[2] = {0, 0};
//                    ConvertDoubleToByteArray(outSample, (float)resampledBuffers[j][i]);
//                    streamDataSampleInBytes(outputStreams[j], outSample);
                
            }
            
//            if (bufferCounter == 200){
//                wComponent.callWhisperFullWithoutAudiofile(ctx, params, floatBuffer.data(), (int)floatBuffer.size(), params.n_processors != 0);
//                bufferCounter = 0;
//                floatBuffer.clear();
//            }
            
            dataRead += inputStream.gcount();
            if (dataRead >= wavFile._datasize){
                for (int i = 0; i < channelCount; i++){
                    outputStreams[i].write(reinterpret_cast<char*>(floatBuffer[i].data()), (int)floatBuffer[i].size() * sizeof(float));
                    outputStreams[i].close();
                    canRead.store(true);
                }
                break;
            }
        }
        std::cout<< "Almost done, closing file streams -  ";
        printElapsedTimeSince(start);
        std::cout<<'\n';
        inputStream.close();
//        for (int i = 0; i < channelCount; i++){
//            // we should write data chunk size in here
//            outputStreams[i].close();
//        }
    }
    
    std::cout << "ALL DONE ON MAIN THREAD -  ";
    printElapsedTimeSince(start);
    whisperThread.join();
    return 0;
}

//    whisper_params _params;
//    // input a single 44.1k 16bit audiofile
//    _params.fname_inp.push_back("/Users/tonytorm/Documents/gCoding/whisp/TESTFILES/TESTWAV1.wav");
//
//    // whisper init
//    params.model = "/Users/tonytorm/Documents/gCoding/whisp/models/ggml-model-whisper-tiny.en.bin";
//    whisper_context* _ctx = whisper_init_from_file(_params.model.c_str());
//
//    if (_ctx == nullptr) {
//        fprintf(stderr, "error: failed to initialize whisper context\n");
//        return 0;
//    }
//    std::cout << '\n' << "Model loaded -  ";
//    printElapsedTimeSince(start);
//    std::cout << '\n';
//
//    // 1.42 secs on 4.41 minutes file
//
//
//    runTranscription(_ctx, _params);
