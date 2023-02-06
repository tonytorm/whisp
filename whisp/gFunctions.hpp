//
//  gFunctions.hpp
//  Whisp_playground
//
//  Created by Giulio Iacomino on 03/02/2023.
//  Copyright © 2023 Giulio Iacomino. All rights reserved.
//

#ifndef gFunctions_hpp
#define gFunctions_hpp

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <vector>

const uint32_t chunk_RIFF = 'RIFF';
const uint32_t chunk_RF64 = 'RF64';
const uint32_t chunk_WAVE = 'WAVE';
//const uint32_t chunk_fmt  = 'fmt ';
const uint32_t chunk_data = 'data';

struct fmtChunk {
    short comp;
    short channels;
    int samplerate;
    int bytesPerSecond;
    short blockAlign;     //bytes per sample * channels
    short bitDepth;
};

inline void writeRIFFChunk(std::ostream& outfile, uint32_t totalFileSize){
    outfile.write("RIFF", 4);
    uint32_t chunkSize = totalFileSize - 8;
    outfile.write((char*)&chunkSize, 4);
    outfile.write("WAVE", 4);
}

inline void writeFMTChunk(std::ostream& outfile,int sr, short bd, short chans){
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
inline void writeDataChunkHeader(std::ostream& outfile, uint32_t dataSize){
    outfile<<"data";
    outfile.write(reinterpret_cast<const char*>(&dataSize),4);
}
inline void streamDataBuffer(std::ostream& outfile, std::vector<double> data){
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
inline void streamDataSampleInBytes(std::ostream& outfile, char* buffer){
    outfile.write(buffer, 1);
    //auto i = buffer+1;
    outfile.write(buffer+1, 1);
}
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

struct AudioFile{
    void readWav(std::ifstream& stream, parseInfo& pInfo){
        uint32_t intTag=0;  //local version
        fmtChunk fmt;
        bool processingUnknownChunk = false;
        
        while (!stream.eof()) {
            stream.read((char*)&intTag, 4);
            if(stream.eof()){
                //cout << "Reached end of file\n";
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
                std::cout << "ERROR!!! tagsize = 0\n";
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
                    //cout<< "Parsing FMT chunk" << '\n';
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
                    //cout<< "Parse data chunk\n";
                    _datasize = _tagsize;

                    if (pInfo.formatChunkPresent) {
                        //if fmt chunk already scanned, calculate samplecount
                        _samples = _datasize/fmt.blockAlign;
                    }
                    
                    _datalocation = (uint32_t)stream.tellg();
                    
                    stream.seekg(_datalocation);
                    stream.seekg(_datasize,std::ios_base::cur);
                    pInfo.dataChunkPresent = true;
                    if(oddTagSize)stream.ignore(1);
                    
                    break;
                }
                default: {
                    //unknown
                    if (processingUnknownChunk) {
                        //this gets triggered second time round, when chunks fail twice
                        pInfo.chunk_misfires++;
                        stream.seekg(-7,std::ios_base::cur);//seek forward one to check for tag and skip back over tag header
                        
                        std::cout<< "TAG MISFIRE "<<pInfo.chunk_misfires<<"!\n";
                        if(pInfo.chunk_misfires>20){
                            std::cout<<"ERROR - Chunk failure in audiofile " <<std::endl;
                            std::cout<<"*** TAGS MISFIRING TOO MUCH - ENDING PARSING PROCESS\n";
                            pInfo.chunksGood=false;
                            return;
                        }
                    }
                    if (pInfo.chunk_misfires == 19) processingUnknownChunk = true;
                    
                    // try next chunk
                    stream.seekg(skipsize,std::ios_base::cur);
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
inline double convertToDouble (int32_t v) noexcept{
    return static_cast<double> (1.0 / 0x7fffff) * static_cast<int32_t> (v);
}

inline double convertByteArrayToDouble (const void* source){
    auto s = static_cast<const uint8_t*> (source);
    auto i = (static_cast<uint32_t> (static_cast<int32_t> (static_cast<int8_t> (s[2]))) << 16) |
                                                           static_cast<uint32_t> (s[1] << 8) |
                                                           static_cast<uint32_t> (s[0]);
    return convertToDouble(static_cast<int32_t> (i));
}

template <typename IntType, int32_t maxVal, typename FloatType>
inline IntType convertToInt (FloatType v) noexcept {
    auto scaled = v * static_cast<FloatType> (maxVal);
    return scaled <= static_cast<FloatType> (-maxVal - 1)
                ? static_cast<IntType> (-maxVal - 1)
                : (scaled >= static_cast<FloatType> (maxVal)
                    ? static_cast<IntType> (maxVal)
                    : static_cast<IntType> (scaled));
}



inline void ConvertDoubleToByteArray (char* dest, double v){     // double to int16_t
    int16_t i = convertToInt<int16_t, 0x7FFF> (v);        // i = 0xABCD
    dest[0] = static_cast<char> (i);                      // dest[0] = AB
    dest[1] = static_cast<char> (i >> 8);                 // dest[1] = CD
}
inline bool readHeader(std::string filepath, AudioFile* audiofile){
    parseInfo pInfo;
    
    std::ifstream inputStream;
    inputStream.open(filepath, std::ifstream::binary);
    if(!inputStream){
        std::cout<< "stream failed to open!\n";
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
        std::cout<< "Audiofile RIFF/RF64 header not present for "<< filepath << '\n';
        inputStream.close();
        return false;
    }
    
    unsigned long totalchunksize=0;
    
    inputStream.read(reinterpret_cast<char*>(&totalchunksize), 4);
    if (totalchunksize==0) {
        std::cout<< "ERROR - total chunk size is zero - possible file closing fault!\n";
        //        return false;
    }
    
    inputStream.read((char*)&intTag, 4);
    chunkType = _OSSwapInt32(intTag);
    
    if (chunkType==chunk_WAVE) {
    }else{
        std::cout <<"WAVE tag missing, returning\n";
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
            std::cout<< "ERROR - Data chunk zero size\n";
        }
        inputStream.close();
        return false;
    }
}
inline void allocateByteArray(char& input, int bytesPerSample, char* bufToWriteTo){
    for (int i = 0; i < bytesPerSample; i++){
        bufToWriteTo[i] = input + i;
    }
}

inline void printElapsedTimeSince(std::chrono::time_point<std::chrono::high_resolution_clock> start){
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    std::cout << "ELAPSED TIME SINCE START: " << elapsed_seconds << " SECONDS\n";
}

#endif /* gFunctions_hpp */
