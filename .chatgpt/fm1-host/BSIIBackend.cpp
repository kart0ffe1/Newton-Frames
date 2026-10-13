// SPDX-License-Identifier: GPL-3.0-only
#include "FirmwareApi.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kRate = 44100;
constexpr int kScreenW = 240, kScreenH = 240;
constexpr int kPresetCount = 128;
constexpr int kParamCount = 32;
constexpr float kPi = 3.14159265358979323846f;

enum Param {
    P_OSC1_WAVE, P_OSC1_RANGE, P_OSC1_COARSE, P_OSC1_FINE,
    P_OSC2_WAVE, P_OSC2_RANGE, P_OSC2_COARSE, P_OSC2_FINE,
    P_OSC_MIX, P_SUB_LEVEL, P_NOISE, P_RING,
    P_CUTOFF, P_RESO, P_ENV_AMT, P_DRIVE,
    P_AMP_A, P_AMP_D, P_AMP_S, P_AMP_R,
    P_MOD_A, P_MOD_D, P_MOD_S, P_MOD_R,
    P_LFO1_RATE, P_LFO1_DEPTH, P_LFO2_RATE, P_LFO2_DEPTH,
    P_GLIDE, P_FILTER_TYPE, P_FILTER_SHAPE, P_FILTER_SLOPE
};

struct Preset {
    uint8_t p[kParamCount];
    char name[16];
};

struct Voice {
    bool gate = false;
    int note = 48;
    int velocity = 100;
    float phase1 = 0.0f, phase2 = 0.0f, phaseSub = 0.0f;
    float envAmp = 0.0f, envMod = 0.0f;
    int ampStage = 0, modStage = 0; // 0 off,1 atk,2 dec,3 sus,4 rel
    float currentHz = 130.8128f;
    float targetHz = 130.8128f;
    // TPT/ZDF SVF integrator-equivalent states. bp is ic1eq, lp1 is ic2eq.
    float lp1 = 0, lp2 = 0, bp = 0;
};

std::array<float, 1024> outL{}, outR{};
std::array<uint16_t, kScreenW * kScreenH> screen{};
std::array<Preset, kPresetCount> flash{};
uint32_t flashWrites = 0;
Preset current{};
Voice voice{};
int presetIndex = 0;
int page = 0;
uint32_t buttonState = 0, prevButtonState = 0;
uint32_t keyState = 0;
int masterValue = 700;
float lfo1Phase = 0, lfo2Phase = 0;
uint32_t rngState = 0x12345678u;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t v = uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return uint16_t((v >> 8) | (v << 8)); // host swaps JIANT-style framebuffer words
}
void px(int x,int y,uint16_t c){ if(x>=0&&x<kScreenW&&y>=0&&y<kScreenH) screen[size_t(y*kScreenW+x)] = c; }
void fillRect(int x,int y,int w,int h,uint16_t c){ for(int yy=y;yy<y+h;++yy) for(int xx=x;xx<x+w;++xx) px(xx,yy,c); }
void line(int x0,int y0,int x1,int y1,uint16_t c){ int dx=std::abs(x1-x0), sx=x0<x1?1:-1, dy=-std::abs(y1-y0), sy=y0<y1?1:-1, e=dx+dy; for(;;){px(x0,y0,c); if(x0==x1&&y0==y1)break; int e2=2*e; if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;}} }

// tiny 5x7 uppercase font, generated from compact bit rows for labels we use.
const uint8_t* glyph(char c) {
    static const uint8_t blank[7]={0,0,0,0,0,0,0};
    static const uint8_t A[7]={14,17,17,31,17,17,17},B[7]={30,17,17,30,17,17,30},C[7]={15,16,16,16,16,16,15},D[7]={30,17,17,17,17,17,30},E[7]={31,16,16,30,16,16,31},F[7]={31,16,16,30,16,16,16},G[7]={15,16,16,19,17,17,14},H[7]={17,17,17,31,17,17,17},I[7]={31,4,4,4,4,4,31},L[7]={16,16,16,16,16,16,31},M[7]={17,27,21,21,17,17,17},N[7]={17,25,21,19,17,17,17},O[7]={14,17,17,17,17,17,14},P[7]={30,17,17,30,16,16,16},R[7]={30,17,17,30,20,18,17},S[7]={15,16,16,14,1,1,30},T[7]={31,4,4,4,4,4,4},U[7]={17,17,17,17,17,17,14},V[7]={17,17,17,17,10,10,4},W[7]={17,17,17,21,21,21,10},Y[7]={17,17,10,4,4,4,4};
    static const uint8_t Z0[7]={14,17,19,21,25,17,14},Z1[7]={4,12,4,4,4,4,14},Z2[7]={14,17,1,2,4,8,31},Z3[7]={30,1,1,14,1,1,30},Z4[7]={2,6,10,18,31,2,2},Z5[7]={31,16,16,30,1,1,30},Z6[7]={14,16,16,30,17,17,14},Z7[7]={31,1,2,4,8,8,8},Z8[7]={14,17,17,14,17,17,14},Z9[7]={14,17,17,15,1,1,14};
    switch(c){case'A':return A;case'B':return B;case'C':return C;case'D':return D;case'E':return E;case'F':return F;case'G':return G;case'H':return H;case'I':return I;case'L':return L;case'M':return M;case'N':return N;case'O':return O;case'P':return P;case'R':return R;case'S':return S;case'T':return T;case'U':return U;case'V':return V;case'W':return W;case'Y':return Y;case'0':return Z0;case'1':return Z1;case'2':return Z2;case'3':return Z3;case'4':return Z4;case'5':return Z5;case'6':return Z6;case'7':return Z7;case'8':return Z8;case'9':return Z9;default:return blank;}
}
void text(int x,int y,const char* s,uint16_t c,int scale=2){ for(;*s;++s){ if(*s==' '){x+=6*scale;continue;} auto g=glyph(*s); for(int r=0;r<7;++r)for(int col=0;col<5;++col)if(g[r]&(1<<(4-col)))fillRect(x+col*scale,y+r*scale,scale,scale,c); x+=6*scale; } }
void number3(int x,int y,int v,uint16_t c){ char b[4]; v=std::clamp(v,0,999); b[0]=char('0'+v/100); b[1]=char('0'+(v/10)%10); b[2]=char('0'+v%10); b[3]=0; text(x,y,b,c,2); }

float midiHz(int n){ return 440.0f*std::pow(2.0f,(float(n)-69.0f)/12.0f); }
float polyBlep(float t,float dt){ if(t<dt){t/=dt;return t+t-t*t-1.0f;} if(t>1.0f-dt){t=(t-1.0f)/dt;return t*t+t+t+1.0f;} return 0.0f; }
float osc(int wave,float& ph,float hz){ float dt=std::min(hz/float(kRate),0.45f), y=0; if(wave==0){y=2*ph-1; y-=polyBlep(ph,dt);} else if(wave==1){y=ph<0.5f?1.0f:-1.0f; y+=polyBlep(ph,dt); float t=std::fmod(ph+0.5f,1.0f); y-=polyBlep(t,dt);} else if(wave==2){y=2.0f*std::fabs(2.0f*ph-1.0f)-1.0f;} else y=std::sin(2*kPi*ph); ph+=dt; if(ph>=1)ph-=1; return y; }
float randNoise(){ rngState = rngState*1664525u+1013904223u; return (float(int32_t(rngState))/2147483648.0f); }
float envTime(uint8_t v,bool attack=false){ float n=float(v)/127.0f; return attack ? (0.002f+4.0f*n*n) : (0.005f+5.0f*n*n); }
void stepEnv(float& e,int& stage,const uint8_t* p,int base){ float target=e, coef=1; if(stage==1){target=1;coef=1.0f/(envTime(p[base],true)*kRate);} else if(stage==2){target=float(p[base+2])/127.0f;coef=1.0f/(envTime(p[base+1])*kRate);} else if(stage==3){e=float(p[base+2])/127.0f;return;} else if(stage==4){target=0;coef=1.0f/(envTime(p[base+3])*kRate);} else {e=0;return;} if(target>e){e=std::min(target,e+coef);} else e=std::max(target,e-coef); if(std::fabs(e-target)<0.001f){e=target;if(stage==1)stage=2;else if(stage==2)stage=3;else if(stage==4)stage=0;} }

void defaults(Preset& p,int i){ std::fill(std::begin(p.p),std::end(p.p),64); p.p[P_OSC1_WAVE]=0;p.p[P_OSC1_RANGE]=64;p.p[P_OSC1_COARSE]=64;p.p[P_OSC1_FINE]=64;p.p[P_OSC2_WAVE]=0;p.p[P_OSC2_RANGE]=64;p.p[P_OSC2_COARSE]=64;p.p[P_OSC2_FINE]=70;p.p[P_OSC_MIX]=64;p.p[P_SUB_LEVEL]=60;p.p[P_NOISE]=4;p.p[P_RING]=0;p.p[P_CUTOFF]=78;p.p[P_RESO]=35;p.p[P_ENV_AMT]=78;p.p[P_DRIVE]=18;p.p[P_AMP_A]=3;p.p[P_AMP_D]=42;p.p[P_AMP_S]=105;p.p[P_AMP_R]=28;p.p[P_MOD_A]=2;p.p[P_MOD_D]=45;p.p[P_MOD_S]=8;p.p[P_MOD_R]=25;p.p[P_LFO1_RATE]=35;p.p[P_LFO1_DEPTH]=8;p.p[P_LFO2_RATE]=25;p.p[P_LFO2_DEPTH]=10;p.p[P_GLIDE]=0;p.p[P_FILTER_TYPE]=0;p.p[P_FILTER_SHAPE]=0;p.p[P_FILTER_SLOPE]=1; std::snprintf(p.name,sizeof(p.name),"BSFM %03d",i+1); }

void loadPreset(){ current=flash[size_t(presetIndex)]; }
void savePreset(){ flash[size_t(presetIndex)]=current; ++flashWrites; }

int knobParam(int k){
    switch(page){
        case 0: {static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE}; return a[k];}
        case 1: {static int a[4]={P_OSC1_WAVE,P_OSC1_RANGE,P_OSC1_COARSE,P_OSC1_FINE};return a[k];}
        case 2: {static int a[4]={P_OSC2_WAVE,P_OSC2_RANGE,P_OSC2_COARSE,P_OSC2_FINE};return a[k];}
        case 3: {static int a[4]={P_OSC_MIX,P_SUB_LEVEL,P_NOISE,P_RING};return a[k];}
        case 4: {static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE};return a[k];}
        case 5: {static int a[4]={P_FILTER_TYPE,P_FILTER_SHAPE,P_FILTER_SLOPE,P_GLIDE};return a[k];}
        case 6: {static int a[4]={P_AMP_A,P_AMP_D,P_AMP_S,P_AMP_R};return a[k];}
        case 7: {static int a[4]={P_MOD_A,P_MOD_D,P_MOD_S,P_MOD_R};return a[k];}
        case 8: {static int a[4]={P_LFO1_RATE,P_LFO1_DEPTH,P_LFO2_RATE,P_LFO2_DEPTH};return a[k];}
        default:return P_CUTOFF;
    }
}
const char* pageName(){ static const char* n[]={"HOME","OSC 1","OSC 2","MIXER","FILTER","FILTER 2","AMP ENV","MOD ENV","LFO"}; return n[std::clamp(page,0,8)]; }
const char* paramName(int p){ static const char* n[kParamCount]={"WAVE","RANGE","COARSE","FINE","WAVE","RANGE","COARSE","FINE","OSC MIX","SUB","NOISE","RING","CUTOFF","RESO","ENV AMT","DRIVE","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","LFO1 RATE","LFO1 DEP","LFO2 RATE","LFO2 DEP","GLIDE","TYPE","SHAPE","SLOPE"}; return n[p]; }

void drawScreen(){
    const auto bg=rgb565(3,8,16), deep=rgb565(5,18,32), blue=rgb565(0,98,185), cyan=rgb565(42,194,255), pale=rgb565(185,230,255), dim=rgb565(55,95,125);
    std::fill(screen.begin(),screen.end(),bg); fillRect(0,0,240,28,deep); text(10,7,"BSFM",pale,2); text(166,7,"P",dim,2); number3(180,7,presetIndex+1,pale);
    text(10,39,pageName(),cyan,2); line(10,58,230,58,blue);
    for(int k=0;k<4;++k){ int p=knobParam(k), x=(k%2)*116+10, y=(k/2)*76+70; text(x,y,paramName(p),dim,1); number3(x,y+18,current.p[p],pale); int w=int(current.p[p]*92/127); fillRect(x,y+39,94,8,deep); fillRect(x,y+39,w,8,blue); }
    if(page==6||page==7){ int base=page==6?P_AMP_A:P_MOD_A; int x0=20,y0=222; float a=current.p[base]/127.0f,d=current.p[base+1]/127.0f,s=current.p[base+2]/127.0f,r=current.p[base+3]/127.0f; line(x0,y0,x0+int(50*a)+8,185,cyan); int x1=x0+int(50*a)+8; line(x1,185,x1+int(50*d)+8,185+int((1-s)*32),cyan); int x2=x1+int(50*d)+8; line(x2,185+int((1-s)*32),190,185+int((1-s)*32),cyan); line(190,185+int((1-s)*32),220,y0,cyan); }
    if(page==4||page==5){ int yy=218; int cut=15+int(current.p[P_CUTOFF]*195/127); for(int x=15;x<225;++x){ int y = x<cut ? yy : yy-int(std::min(31,(x-cut)*(current.p[P_FILTER_SLOPE]?2:1))/2); px(x,y,cyan); } }
    if(page==1||page==2){ int p=page==1?P_OSC1_WAVE:P_OSC2_WAVE; int wave=current.p[p]/32; for(int x=15;x<225;++x){ float t=float(x-15)/35.0f; float yv= wave==0 ? 2*std::fmod(t,1.0f)-1 : wave==1 ? (std::fmod(t,1.0f)<.5f?1:-1) : wave==2 ? 2*std::fabs(2*std::fmod(t,1.0f)-1)-1 : std::sin(2*kPi*t); px(x,215-int(yv*14),cyan);} }
}

void noteOn(int n,int vel){ voice.note=n;voice.velocity=vel;voice.targetHz=midiHz(n);if(!voice.gate||current.p[P_GLIDE]==0)voice.currentHz=voice.targetHz;voice.gate=true;voice.ampStage=1;voice.modStage=1; }
void noteOff(int n){ if(n==voice.note){voice.gate=false;voice.ampStage=4;voice.modStage=4;} }
void handleButtons(uint32_t now){ uint32_t pressed=now & ~prevButtonState; prevButtonState=now; if(pressed&(1u<<6))page=0; if(pressed&(1u<<1))page=(page==1?2:1); if(pressed&(1u<<4))page=3; if(pressed&(1u<<0))page=(page==4?5:4); if(pressed&(1u<<2))page=(page==6?7:6); if(pressed&(1u<<3))page=8; if(pressed&(1u<<7))savePreset(); if(pressed&(1u<<12)) voice.note-=12,voice.targetHz=midiHz(voice.note); if(pressed&(1u<<13)) voice.note+=12,voice.targetHz=midiHz(voice.note); drawScreen(); }

float rangeMul(uint8_t v){ int idx=std::clamp(int(v)*4/128,0,3); static float m[4]={0.25f,0.5f,1.0f,2.0f}; return m[idx]; }
void resetFilter(){ voice.lp1=0.0f; voice.lp2=0.0f; voice.bp=0.0f; }
float renderOne(){
    stepEnv(voice.envAmp,voice.ampStage,current.p,P_AMP_A); stepEnv(voice.envMod,voice.modStage,current.p,P_MOD_A);
    float glide=current.p[P_GLIDE]/127.0f; if(glide>0){ float tau=0.01f+2.5f*glide*glide; voice.currentHz += (voice.targetHz-voice.currentHz)*(1.0f/(tau*kRate)); }
    float lfo1hz=0.03f+18.0f*std::pow(current.p[P_LFO1_RATE]/127.0f,2.0f); float lfo2hz=0.03f+18.0f*std::pow(current.p[P_LFO2_RATE]/127.0f,2.0f); lfo1Phase=std::fmod(lfo1Phase+lfo1hz/kRate,1.0f); lfo2Phase=std::fmod(lfo2Phase+lfo2hz/kRate,1.0f); float l1=std::sin(2*kPi*lfo1Phase),l2=std::sin(2*kPi*lfo2Phase);
    float pitchLfo=l1*(current.p[P_LFO1_DEPTH]/127.0f)*0.5f; float hz1=voice.currentHz*rangeMul(current.p[P_OSC1_RANGE])*std::pow(2.0f,((int(current.p[P_OSC1_COARSE])-64)/12.0f)+((int(current.p[P_OSC1_FINE])-64)/1200.0f)+pitchLfo/12.0f); float hz2=voice.currentHz*rangeMul(current.p[P_OSC2_RANGE])*std::pow(2.0f,((int(current.p[P_OSC2_COARSE])-64)/12.0f)+((int(current.p[P_OSC2_FINE])-64)/1200.0f)-pitchLfo/12.0f);
    int w1=std::min(3,int(current.p[P_OSC1_WAVE]/32)),w2=std::min(3,int(current.p[P_OSC2_WAVE]/32)); float a=osc(w1,voice.phase1,hz1),b=osc(w2,voice.phase2,hz2); float mix=current.p[P_OSC_MIX]/127.0f; float s=a*(1-mix)+b*mix; s += osc(1,voice.phaseSub,voice.currentHz*0.5f)*(current.p[P_SUB_LEVEL]/127.0f)*0.7f; s += randNoise()*(current.p[P_NOISE]/127.0f)*0.25f; s += (a*b)*(current.p[P_RING]/127.0f)*0.35f;
    float drive=1.0f+8.0f*std::pow(current.p[P_DRIVE]/127.0f,2.0f); s=std::tanh(s*drive)/std::tanh(drive);

    const float baseCut=35.0f*std::pow(18000.0f/35.0f,current.p[P_CUTOFF]/127.0f);
    const float envSemis=(int(current.p[P_ENV_AMT])-64)*0.45f*voice.envMod;
    const float lfoSemis=l2*(current.p[P_LFO2_DEPTH]/127.0f)*12.0f;
    const float cutoff=std::clamp(baseCut*std::pow(2.0f,(envSemis+lfoSemis)/12.0f),30.0f,18000.0f);

    // Topology-preserving-transform state-variable filter. Unlike the previous
    // forward-Euler/Chamberlin form this remains stable when cutoff is swept
    // rapidly near Nyquist while resonance is high.
    const float g=std::tan(kPi*cutoff/float(kRate));
    const float resNorm=current.p[P_RESO]/127.0f;
    const float q=0.5f+9.5f*resNorm*resNorm;
    const float damping=1.0f/q;
    const float a1=1.0f/(1.0f+g*(g+damping));
    const float a2=g*a1;
    const float a3=g*a2;
    const float ic1=voice.bp;
    const float ic2=voice.lp1;
    const float v3=s-ic2;
    const float band=a1*ic1+a2*v3;
    const float low=ic2+a2*ic1+a3*v3;
    voice.bp=2.0f*band-ic1;
    voice.lp1=2.0f*low-ic2;
    const float high=s-damping*band-low;

    float filtered=low;
    if(current.p[P_FILTER_SHAPE]>84)filtered=high;
    else if(current.p[P_FILTER_SHAPE]>42)filtered=band;

    // Prototype 24 dB mode: a stable extra pole after the main SVF. A proper
    // BSII-style four-pole model can replace this later without changing UI.
    if(current.p[P_FILTER_SLOPE]){
        const float alpha=1.0f-std::exp(-2.0f*kPi*cutoff/float(kRate));
        voice.lp2 += alpha*(filtered-voice.lp2);
        filtered=voice.lp2;
    }

    // A corrupted filter state must never permanently mute an instance.
    if(!std::isfinite(voice.lp1)||!std::isfinite(voice.lp2)||!std::isfinite(voice.bp)||!std::isfinite(filtered)
       ||std::fabs(voice.lp1)>1000.0f||std::fabs(voice.lp2)>1000.0f||std::fabs(voice.bp)>1000.0f){
        resetFilter();
        filtered=0.0f;
    }

    float vel=0.35f+0.65f*(voice.velocity/127.0f);
    float result=filtered*voice.envAmp*vel*(masterValue/1023.0f)*0.45f;
    if(!std::isfinite(result)){ resetFilter(); result=0.0f; }
    return result;
}
}

extern "C" {
static float* b_out_l(){return outL.data();} static float* b_out_r(){return outR.data();}
static void b_render(uint32_t n){ n=std::min<uint32_t>(n,1024); for(uint32_t i=0;i<n;++i){float s=renderOne();outL[i]=outR[i]=s;} }
static uint8_t* b_nor(){return reinterpret_cast<uint8_t*>(flash.data());} static uint32_t b_nor_size(){return uint32_t(sizeof(flash));} static void b_nor_erase(){std::memset(flash.data(),0,sizeof(flash)); for(int i=0;i<kPresetCount;++i)defaults(flash[size_t(i)],i);} static uint32_t b_flash_writes(){return flashWrites;}
static void b_seed(uint32_t s){rngState=s?s:0x12345678u;} static void b_boot(){ bool blank=true; for(auto&pr:flash)if(pr.name[0]){blank=false;break;} if(blank)b_nor_erase();presetIndex=0;loadPreset();voice=Voice{};drawScreen(); }
static void b_buttons(uint32_t m){buttonState=m;handleButtons(m);} static void b_keys(uint32_t m){ uint32_t changed=m^keyState; for(int i=0;i<27;++i)if(changed&(1u<<i)){int n=53+i;if(m&(1u<<i))noteOn(n,105);else noteOff(n);} keyState=m; }
static void b_enc(uint32_t role,int32_t n){ if(role==0){page=std::clamp(page+n,0,8);} else if(role==1){page=std::clamp(page+n,0,8);} else if(role==2){presetIndex=(presetIndex+n)%kPresetCount;if(presetIndex<0)presetIndex+=kPresetCount;loadPreset();resetFilter();} else if(role>=3&&role<=6){int p=knobParam(int(role-3));current.p[p]=uint8_t(std::clamp(int(current.p[p])+int(n),0,127));} drawScreen(); }
static void b_master(uint32_t v){masterValue=int(std::min<uint32_t>(v,1023));}
static uint32_t b_midi(uint32_t st,uint32_t d1,uint32_t d2){uint8_t s=uint8_t(st); if((s&0xF0)==0x90&&d2>0)noteOn(int(d1),int(d2)); else if((s&0xF0)==0x80||((s&0xF0)==0x90&&d2==0))noteOff(int(d1)); else if((s&0xF0)==0xB0){if(d1==16)current.p[P_CUTOFF]=uint8_t(d2);else if(d1==82)current.p[P_RESO]=uint8_t(d2);else if(d1==20)current.p[P_DRIVE]=uint8_t(d2);drawScreen();} return 0;}
static uint16_t* b_screen(){return screen.data();} static uint32_t b_lit(){return keyState;} static uint32_t b_mid(){return 0;} static uint32_t b_rate(){return kRate;}

#if defined(_WIN32)
__declspec(dllexport)
#endif
const FM1BackendApiV1* fm1_backend_get_api(void){ static FM1BackendApiV1 api{FM1_BACKEND_ABI_VERSION,"bsii_proto","BSII FM-1 Prototype",b_out_l,b_out_r,b_render,b_nor,b_nor_size,b_nor_erase,b_flash_writes,b_seed,b_boot,b_buttons,b_keys,b_enc,b_master,b_midi,b_screen,b_lit,b_mid,b_rate}; return &api; }
}
