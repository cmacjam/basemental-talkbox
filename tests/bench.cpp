#include "../Source/dsp/TalkBoxEngine.h"
#include <chrono>
#include <cstdio>
using namespace btb;
int main(){ for(int poly=0;poly<2;++poly){ TalkBoxEngine e; EngineParams p; p.synth.poly=poly; p.doublerOn=p.roomOn=true; e.setParams(p); e.prepare(48000,512);
 for(int n=0;n<(poly?6:1);++n) e.noteOn(48+n*4,0.8f); std::vector<float> v(512),L(512),R(512); Noise nz; for(auto&x:v) x=0.2f*nz.next();
 auto t0=std::chrono::steady_clock::now(); int blocks=48000*10/512; for(int b=0;b<blocks;++b) e.process(v.data(),L.data(),R.data(),512);
 double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); printf("%s: %.1f%% of one core\n", poly?"poly 6 notes":"mono", 100*s/10.0);} }
