//#include "WhisperComponent.hpp"

#include <stdlib.h>

//#include "WAVData.hpp"
#include "resamplingCode/CDSPResampler.h"


// /Users/tonytorm/Desktop/HH/rachel_0.8.mp3
// /Users/tonytorm/Desktop/kraken_media/17Y07M04/S30T01.WAV   few file paths

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

const uint32_t chunk_RIFF = 'RIFF';
const uint32_t chunk_RF64 = 'RF64';
const uint32_t chunk_WAVE = 'WAVE';
const uint32_t chunk_fmt  = 'fmt ';
const uint32_t chunk_data = 'data';

using namespace std;

struct parseInfo{
    unsigned int chunk_misfires=0;
    bool chunksGood = false;
    bool formatChunkPresent = false;
    bool dataChunkPresent = false;
    bool dataChunkSizeZeroFault = false;
    bool isRF64 = false;
    uint64_t RF64riffSize;
    uint64_t RF64dataSize;
    uint64_t RF64sampleCount;
};
struct fmtChunk {
    short comp;
    short channels;
    int samplerate;
    int bytesPerSecond;
    short blockAlign;     //bytes per sample * channels
    short bitDepth;
};

void writeRIFFChunk(ostream& outfile, uint32_t totalFileSize){
    outfile.write("RIFF", 4);
    uint32_t chunkSize = totalFileSize - 8;
    outfile.write((char*)&chunkSize, 4);
    outfile.write("WAVE", 4);
}

void writeFMTChunk(ostream& outfile,int sr, short bd, short chans){
    fmtChunk fmt;
    fmt.comp=1;
    fmt.channels=chans;
    fmt.samplerate=sr;
    fmt.bitDepth=bd;
    fmt.blockAlign = chans*(bd/8);
    fmt.bytesPerSecond=sr*fmt.blockAlign;
    
    if(bd==32)fmt.comp=3;
    
    int fmtsize = 16;

    outfile<<"fmt ";
    outfile.write(reinterpret_cast<const char*>(&fmtsize),4);
    outfile.write(reinterpret_cast<const char*>(&fmt),fmtsize);
}
void writeDataChunkHeader(ostream& outfile, uint32_t dataSize){
    outfile<<"data";
    outfile.write(reinterpret_cast<const char*>(&dataSize),4);
}
void streamDataBuffer(ostream& outfile, std::vector<double> data){
   int bytesPerSample = 2;
   for (double value : data) {
       int16_t sample = (int16_t)(value * (double)(1 << (8 * bytesPerSample - 1)));
       char buffer[bytesPerSample];
       for (int i = 0; i < bytesPerSample; i++) {
           buffer[i] = (sample >> (8 * i)) & 0xff;
       }
       outfile.write(buffer, bytesPerSample);
   }
}
void streamDataSampleInBytes(ostream& outfile, char* buffer){
    outfile.write(buffer, 1);
    //auto i = buffer+1;
    outfile.write(buffer+1, 1);
}
struct AudioFile{
    void readWav(ifstream& stream, parseInfo& pInfo){
        uint32_t intTag=0;  //local version
        fmtChunk fmt;
        bool processingUnknownChunk = false;
        
        while (!stream.eof()) {
            stream.read((char*)&intTag, 4);
            if(stream.eof()){
                cout << "Reached end of stream - breaking\n";
                break;
            }
            char tag[5] = {0};
            memcpy(tag, &intTag, 4);
            
            unsigned int _tagsize;
            unsigned int skipsize;
            
            stream.read(reinterpret_cast<char*>(&_tagsize), 4);
            
            skipsize=_tagsize;
            
            
            bool oddTagSize = false;
            if((_tagsize&1) == 1){
                //it seems that RIFF chunks should be even number sizes, but some implementations fail with this
                //https://github.com/mono/taglib-sharp/issues/76
                oddTagSize=true;
                skipsize++;
            }
            
            uint32_t chunkType = _OSSwapInt32(intTag);
            
            if(_tagsize==0){
                cout << "ERROR!!! tagsize = 0\n";
                if (chunkType==chunk_data) {
                    this->_datasize=0;
                    pInfo.chunksGood=false;
                    pInfo.dataChunkSizeZeroFault = true;
                    pInfo.dataChunkPresent = true;
                }
                continue;
            }
            
            switch(chunkType){
                case 'fmt ': /*chunk_fmt:*/ {
                    cout<< "Parsiing FMT chunk" << '\n';
                    stream.read((char*)&fmt, sizeof(fmt));
                    
                    _samplerate = fmt.samplerate;
                    _bitdepth = fmt.bitDepth;
                    _ChannelCount = fmt.channels;
                    _format = fmt.comp;
                    
                    if (_tagsize>16){stream.ignore(_tagsize-16);}
                    if (_datasize!=0) {
                        //if data chunk already scanned, calculate samplecount
                        _samples = _datasize/fmt.blockAlign;
                    }
                    pInfo.formatChunkPresent=true;
                    if(fmt.samplerate==0||fmt.channels==0||fmt.bitDepth==0){
                        pInfo.formatChunkPresent = false;
                    }
                    
                    if(oddTagSize)stream.ignore(1);
                    break;
                }
                case chunk_data: {
                    cout<< "Parse data chunk\n";
                    _datasize = _tagsize;

                    if (pInfo.formatChunkPresent) {
                        //if fmt chunk already scanned, calculate samplecount
                        _samples = _datasize/fmt.blockAlign;
                    }
                    
                    _datalocation = (uint32_t)stream.tellg();
                    
                    stream.seekg(_datalocation);
                    stream.seekg(_datasize,ios_base::cur);
                    pInfo.dataChunkPresent = true;
                    if(oddTagSize)stream.ignore(1);
                    
                    break;
                }
                default: {
                    //unknown
                    if (processingUnknownChunk) {
                        //this gets triggered second time round, when chunks fail twice
                        pInfo.chunk_misfires++;
                        stream.seekg(-7,ios_base::cur);//seek forward one to check for tag and skip back over tag header
                        
                        cout<< "TAG MISFIRE "<<pInfo.chunk_misfires<<"!\n";
                        if(pInfo.chunk_misfires>20){
                            cout<<"ERROR - Chunk failure in audiofile " <<endl;
                            cout<<"*** TAGS MISFIRING TOO MUCH - ENDING PARSING PROCESS\n";
                            pInfo.chunksGood=false;
                            return;
                        }
                    }
                    if (pInfo.chunk_misfires == 19) processingUnknownChunk = true;
                    
                    // try next chunk
                    stream.seekg(skipsize,ios_base::cur);
                }
            }
        }
    }
    
    uint32_t _samplerate=0;
    short _bitdepth=0;
    short _format=0;                //1=pcm, 3=float
    uint64_t _samples=0;               //length
    uint64_t _datalocation;
    uint64_t _datasize=0;
    int _ChannelCount;
    
   ;
};
double convertToDouble (int32_t v) noexcept{
    return static_cast<double> (1.0 / 0x7fffff) * static_cast<int32_t> (v);
}

double convertByteArrayToDouble (const void* source){
    auto s = static_cast<const uint8_t*> (source);
    auto i = (static_cast<uint32_t> (static_cast<int32_t> (static_cast<int8_t> (s[2]))) << 16) |
                                                           static_cast<uint32_t> (s[1] << 8) |
                                                           static_cast<uint32_t> (s[0]);
    return convertToDouble(static_cast<int32_t> (i));
}

template <typename IntType, int32_t maxVal, typename FloatType>
IntType convertToInt (FloatType v) noexcept {
    auto scaled = v * static_cast<FloatType> (maxVal);
    return scaled <= static_cast<FloatType> (-maxVal - 1)
                ? static_cast<IntType> (-maxVal - 1)
                : (scaled >= static_cast<FloatType> (maxVal)
                    ? static_cast<IntType> (maxVal)
                    : static_cast<IntType> (scaled));
}

void ConvertDoubleToByteArray (char* dest, double v){     // double to int16_t
    int16_t i = convertToInt<int16_t, 0x7FFF> (v);        // i = 0xABCD
    dest[0] = static_cast<char> (i);                      // dest[0] = AB
    dest[1] = static_cast<char> (i >> 8);                 // dest[1] = CD
}
bool readHeader(string filepath, AudioFile* audiofile){
    parseInfo pInfo;
    
    ifstream inputStream;
    inputStream.open(filepath, ifstream::binary);
    if(!inputStream){
        cout<< "stream failed to open!\n";
        inputStream.close();
        return false;
    }
    uint32_t intTag=0;
    char _tag[5] = {0};
    
    inputStream.read((char*)&intTag, 4);
    memcpy(_tag, &intTag, 4);
    uint32_t chunkType = _OSSwapInt32(intTag);
    
    if (chunkType==chunk_RIFF){
        //if wavefile RIFF header  present
    }else if (chunkType==chunk_RF64){
        //RF64 header
        pInfo.isRF64 = true;
    }else{
        cout<< "Audiofile RIFF/RF64 header not present for "<< filepath << '\n';
        inputStream.close();
        return false;
    }
    
    unsigned long totalchunksize=0;
    
    inputStream.read(reinterpret_cast<char*>(&totalchunksize), 4);
    cout<< "total chunk size: "<<totalchunksize << '\n';
    
    if (totalchunksize==0) {
        cout<< "ERROR - total chunk size is zero - possible file closing fault!\n";
        //        return false;
    }
    
    inputStream.read((char*)&intTag, 4);
    chunkType = _OSSwapInt32(intTag);
    
    if (chunkType==chunk_WAVE) {
        cout<<"IS A WAVE FILE\n";
    }else{
        cout <<"WAVE tag missing\n";
        inputStream.close();
        return false;
    }
    
    audiofile->readWav(inputStream, pInfo);
    
    if (pInfo.formatChunkPresent && pInfo.dataChunkPresent){
        inputStream.close();
        return true;
    }else{
        //check data size zero
        if (pInfo.dataChunkSizeZeroFault) {
            cout<< "ERROR - Data chunk zero size\n";
        }
        inputStream.close();
        return false;
    }
}
void allocateByteArray(char& input, int bytesPerSample, char* bufToWriteTo){
    for (int i = 0; i < bytesPerSample; i++){
        bufToWriteTo[i] = input + i;
    }
}

int main(){
    const double OutSampleRate = 16000.0;
    AudioFile wavFile;
    auto filePath = "/Users/tonytorm/Desktop/kraken_media/bal/MIXER-A-T01.WAV";
    if (readHeader(filePath, &wavFile)){
        ifstream inputStream;
        std::vector<ofstream> outputStreams;
        inputStream.open(filePath, ifstream::binary); //49152
        if(!inputStream.good()){
            cout << "error while trying to open buffer\n";
            return -1; // error
        }
        
        inputStream.seekg((uint32_t)wavFile._datalocation);

        uint32_t dataChunkSize = (uint32_t)(wavFile._datasize/wavFile._ChannelCount) /3 * 2; // force to 16 bit depth
        uint32_t fmtChunkSize = 16;
        uint32_t totalFileSize = (8 + fmtChunkSize) + (8 + dataChunkSize);
        for (int i = 0; i < wavFile._ChannelCount; i++){
            std::stringstream ss;
            ss << "/Users/tonytorm/Desktop/TESTWAV" << i << ".wav";
            const std::string str = ss.str();
            const char* outputFileName = str.c_str();
            outputStreams.emplace_back(ofstream(outputFileName, ofstream::binary));
            ofstream& stream = outputStreams.back();
            writeRIFFChunk(stream, totalFileSize);
            writeFMTChunk(stream, 48000, 16, 1);    // write a 16bit mono file for each channel
            writeDataChunkHeader(stream, dataChunkSize);
        }
        
        const int InBufCapacity = 1024;
        int channelCount = wavFile._ChannelCount;
        
        std::vector<r8b::CDSPResampler24*> resamplers;
        int bytesPerSample = wavFile._bitdepth/8;
        int frameSize = bytesPerSample*wavFile._ChannelCount;  // 1 sample for each channel
        const size_t INPUTBUFFERSIZE = frameSize * InBufCapacity;
        std::vector<std::vector<double>> convertedPlanarBuffers; // from raw data to double

        for (int i = 0; i < channelCount; i++){       // allocate a buffer for each channel (planar)
            convertedPlanarBuffers.push_back(std::vector<double>(1024));
            resamplers.push_back(new r8b::CDSPResampler24(48000.0, OutSampleRate, InBufCapacity));
        }
        char buffer[INPUTBUFFERSIZE];                          // allocate an interleaved buffer
        int dataRead = 0;
        std::cout << "nb of required buffers: " << wavFile._datasize / INPUTBUFFERSIZE << '\n';
        while (inputStream.read(buffer, INPUTBUFFERSIZE)) { // buffer speed
            static int counter;
            std::cout << "processing buffer nb: " << counter++ << '\n';
            int b = 0;
            // should we zero the buffers as well?
            for (int i = 0; i < INPUTBUFFERSIZE; i+=frameSize){  // frame speed
                for (int j = 0; j < wavFile._ChannelCount; j++){  // sample speed
                    char rawData[bytesPerSample];
                    for (int z = 0; z < bytesPerSample; z++){
                        rawData[z] = buffer[i + (j*bytesPerSample+z)];
                    }
                    double sample = convertByteArrayToDouble(rawData);
                    convertedPlanarBuffers.at(j).at(b) = sample;

                    char outSample[2] = {0, 0};   // alloc space for 16bit sample - we can hardcode for now
                    ConvertDoubleToByteArray(outSample, convertedPlanarBuffers.at(j).at(b));
                    streamDataSampleInBytes(outputStreams[j], outSample);
                }
                b++;  // this counter is here to
            }
            //buffer speed
            
//            double* resampledBuffers[ wavFile._ChannelCount ];
//
//            int writeCount;
//            int bufferSize = 1024;
//            int maxOutputBufferLength = resampler.getMaxOutLen(bufferSize);
//            writeCount = resampler.process(convertedPlanarBuffers[1].data(), bufferSize, resampledBuffers[1]);
//
//            for (int i = 0; i < writeCount; i++){
//                char outSample[2] = {0, 0};
//                ConvertDoubleToByteArray(outSample, resampledBuffers[1][i]);
//                streamDataSampleInBytes(outputStreams[1], outSample);
//            }
            
            dataRead += inputStream.gcount();
            if (dataRead >= wavFile._datasize){
                break;
            }
        }
        inputStream.close();
        for (int i = 0; i < wavFile._ChannelCount; i++){
            outputStreams[i].close();
        }
    }
//        whisper_params params;
//        // input a single 44.1k 16bit audiofile
//        params.fname_inp.push_back("/Users/tonytorm/Desktop/TESTWAV1.wav");
//
//        // whisper init
//        params.model = "/Users/tonytorm/Documents/gCoding/whisp/models/ggml-model-whisper-tiny.en.bin";
//        whisper_context* ctx = whisper_init_from_file(params.model.c_str());
//
//        if (ctx == nullptr) {
//            fprintf(stderr, "error: failed to initialize whisper context\n");
//            return 0;
//        }
//
//        runTranscription(ctx, params);
    
    return 0;
}
