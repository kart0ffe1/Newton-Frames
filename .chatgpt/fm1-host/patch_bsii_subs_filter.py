from pathlib import Path
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("BSIIBackend.cpp")
s = path.read_text(encoding="utf-8")
orig = s

def rep(old, new, label):
    global s
    if old not in s:
        if new in s:
            return
        raise SystemExit(f"could not patch {label}")
    s = s.replace(old, new, 1)

# Extend the already-migrated mixer storage without changing the 48-byte preset size.
rep(
'''constexpr uint8_t kMixerStorageVersion = 0xB2;\nconstexpr int kNoParam = -1;\nconstexpr int kVirtualOsc2Level = -2;''',
'''constexpr uint8_t kMixerStorageVersion = 0xB3;\nconstexpr int kNoParam = -1;\nconstexpr int kVirtualOsc2Level = -2;\nconstexpr int kVirtualSub2Level = -3;''',
"mixer storage v3")

rep(
'''    float phase1 = 0.0f, phase2 = 0.0f, phaseSub = 0.0f;''',
'''    float phase1 = 0.0f, phase2 = 0.0f, phaseSub1 = 0.0f, phaseSub2 = 0.0f;''',
"separate sub phases")
rep(
'''    float lp1 = 0, lp2 = 0, bp = 0;''',
'''    float lp1 = 0, lp2 = 0, bp = 0, bp2 = 0;''',
"second SVF state")

rep(
'''struct Preset {\n    uint8_t p[kParamCount];\n    // Keep the struct exactly 48 bytes so existing flash.bin files remain readable.\n    // The final two bytes used to be unused tail bytes of name[16].\n    char name[14];\n    uint8_t mixerVersion = 0;\n    uint8_t osc2Level = 64;\n};''',
'''struct Preset {\n    uint8_t p[kParamCount];\n    // Keep byte offsets 46/47 for mixerVersion/osc2Level so v0.1.3 flash stays readable.\n    // Byte 45 was still unused name padding and now stores SUB2 level.\n    char name[13];\n    uint8_t sub2Level = 0;\n    uint8_t mixerVersion = 0;\n    uint8_t osc2Level = 64;\n};''',
"preset layout with sub2")

old_defaults = '''void defaults(Preset& p,int i){ std::fill(std::begin(p.p),std::end(p.p),64); p.p[P_OSC1_WAVE]=0;p.p[P_OSC1_RANGE]=64;p.p[P_OSC1_COARSE]=64;p.p[P_OSC1_FINE]=64;p.p[P_OSC2_WAVE]=0;p.p[P_OSC2_RANGE]=64;p.p[P_OSC2_COARSE]=64;p.p[P_OSC2_FINE]=70;p.p[P_OSC1_LEVEL]=63;p.p[P_SUB_LEVEL]=60;p.p[P_NOISE]=4;p.p[P_RING]=0;p.p[P_CUTOFF]=78;p.p[P_RESO]=35;p.p[P_ENV_AMT]=78;p.p[P_DRIVE]=18;p.p[P_AMP_A]=3;p.p[P_AMP_D]=42;p.p[P_AMP_S]=105;p.p[P_AMP_R]=28;p.p[P_MOD_A]=2;p.p[P_MOD_D]=45;p.p[P_MOD_S]=8;p.p[P_MOD_R]=25;p.p[P_LFO1_RATE]=35;p.p[P_LFO1_DEPTH]=8;p.p[P_LFO2_RATE]=25;p.p[P_LFO2_DEPTH]=10;p.p[P_GLIDE]=0;p.p[P_FILTER_TYPE]=0;p.p[P_FILTER_SHAPE]=0;p.p[P_FILTER_SLOPE]=127;p.mixerVersion=kMixerStorageVersion;p.osc2Level=64; std::snprintf(p.name,sizeof(p.name),"BSFM %03d",i+1); }\nvoid migrateMixerStorage(Preset& p){\n    if(p.mixerVersion==kMixerStorageVersion)return;\n    const uint8_t oldCrossfade=p.p[P_OSC1_LEVEL];\n    p.p[P_OSC1_LEVEL]=uint8_t(127u-oldCrossfade);\n    p.osc2Level=oldCrossfade;\n    p.mixerVersion=kMixerStorageVersion;\n}\nvoid loadPreset(){current=flash[size_t(presetIndex)];migrateMixerStorage(current);}\nvoid savePreset(){current.mixerVersion=kMixerStorageVersion;flash[size_t(presetIndex)]=current;++flashWrites;}'''
new_defaults = '''void defaults(Preset& p,int i){ std::fill(std::begin(p.p),std::end(p.p),64); p.p[P_OSC1_WAVE]=0;p.p[P_OSC1_RANGE]=64;p.p[P_OSC1_COARSE]=64;p.p[P_OSC1_FINE]=64;p.p[P_OSC2_WAVE]=0;p.p[P_OSC2_RANGE]=64;p.p[P_OSC2_COARSE]=64;p.p[P_OSC2_FINE]=70;p.p[P_OSC1_LEVEL]=63;p.p[P_SUB_LEVEL]=52;p.p[P_NOISE]=4;p.p[P_RING]=0;p.p[P_CUTOFF]=78;p.p[P_RESO]=35;p.p[P_ENV_AMT]=78;p.p[P_DRIVE]=18;p.p[P_AMP_A]=3;p.p[P_AMP_D]=42;p.p[P_AMP_S]=105;p.p[P_AMP_R]=28;p.p[P_MOD_A]=2;p.p[P_MOD_D]=45;p.p[P_MOD_S]=8;p.p[P_MOD_R]=25;p.p[P_LFO1_RATE]=35;p.p[P_LFO1_DEPTH]=8;p.p[P_LFO2_RATE]=25;p.p[P_LFO2_DEPTH]=10;p.p[P_GLIDE]=0;p.p[P_FILTER_TYPE]=0;p.p[P_FILTER_SHAPE]=0;p.p[P_FILTER_SLOPE]=127;p.sub2Level=0;p.mixerVersion=kMixerStorageVersion;p.osc2Level=64; std::snprintf(p.name,sizeof(p.name),"BSFM %03d",i+1); }\nvoid migrateMixerStorage(Preset& p){\n    if(p.mixerVersion==kMixerStorageVersion)return;\n    if(p.mixerVersion==0xB2){p.sub2Level=0;p.mixerVersion=kMixerStorageVersion;return;}\n    const uint8_t oldCrossfade=p.p[P_OSC1_LEVEL];\n    p.p[P_OSC1_LEVEL]=uint8_t(127u-oldCrossfade);\n    p.osc2Level=oldCrossfade;\n    p.sub2Level=0;\n    p.mixerVersion=kMixerStorageVersion;\n}\nvoid loadPreset(){current=flash[size_t(presetIndex)];migrateMixerStorage(current);}\nvoid savePreset(){current.mixerVersion=kMixerStorageVersion;flash[size_t(presetIndex)]=current;++flashWrites;}'''
rep(old_defaults, new_defaults, "sub2 defaults and migration")

rep(
'''        case 3:{static int a[4]={P_OSC1_LEVEL,kVirtualOsc2Level,P_SUB_LEVEL,P_NOISE};return a[k];}\n        case 4:{static int a[4]={P_RING,kNoParam,kNoParam,kNoParam};return a[k];}''',
'''        case 3:{static int a[4]={P_OSC1_LEVEL,kVirtualOsc2Level,P_SUB_LEVEL,kVirtualSub2Level};return a[k];}\n        case 4:{static int a[4]={P_NOISE,P_RING,kNoParam,kNoParam};return a[k];}''',
"mixer pages with two subs")

rep(
'''    if(p==kVirtualOsc2Level)return "OSC2";\n    if(p==kNoParam)return "";\n    static const char* n[kParamCount]={"WAVE","RANGE","COARSE","FINE","WAVE","RANGE","COARSE","FINE","OSC1","SUB","NOISE","RING","CUTOFF","RESO","ENV AMT","DRIVE","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","LFO1 RATE","LFO1 DEP","LFO2 RATE","LFO2 DEP","GLIDE","TYPE","SHAPE","SLOPE"}; return n[p];\n}\nuint8_t paramRawValue(int p){if(p==kVirtualOsc2Level)return current.osc2Level;if(p>=0&&p<kParamCount)return current.p[p];return 0;}''',
'''    if(p==kVirtualOsc2Level)return "OSC2";\n    if(p==kVirtualSub2Level)return "SUB2";\n    if(p==kNoParam)return "";\n    static const char* n[kParamCount]={"WAVE","RANGE","COARSE","FINE","WAVE","RANGE","COARSE","FINE","OSC1","SUB1","NOISE","RING","CUTOFF","RESO","ENV AMT","DRIVE","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","LFO1 RATE","LFO1 DEP","LFO2 RATE","LFO2 DEP","GLIDE","TYPE","SHAPE","SLOPE"}; return n[p];\n}\nuint8_t paramRawValue(int p){if(p==kVirtualOsc2Level)return current.osc2Level;if(p==kVirtualSub2Level)return current.sub2Level;if(p>=0&&p<kParamCount)return current.p[p];return 0;}''',
"sub labels and values")

rep(
'''    if(p==kVirtualOsc2Level){std::snprintf(b,n,"%d%%",percent(v));return;}''',
'''    if(p==kVirtualOsc2Level||p==kVirtualSub2Level){std::snprintf(b,n,"%d%%",percent(v));return;}''',
"sub2 formatting")

rep(
'''    if(p==kVirtualOsc2Level){current.osc2Level=uint8_t(std::clamp(int(current.osc2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}''',
'''    if(p==kVirtualOsc2Level){current.osc2Level=uint8_t(std::clamp(int(current.osc2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualSub2Level){current.sub2Level=uint8_t(std::clamp(int(current.sub2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}''',
"sub2 editing")

# Filter response display uses the same bilinear/TPT mapping and Q values as the audio filter.
insert_before_draw = '''float filterQ1(float r,bool acid){return 0.68f+(acid?15.0f:12.0f)*std::pow(r,2.25f);}\nfloat filterQ2(float r,bool acid){return 0.72f+(acid?2.8f:1.8f)*r*r;}\nfloat filterStageMagnitude(float freq,float cutoff,float q,int shape){\n    const float gf=std::tan(kPi*std::clamp(freq,20.0f,20000.0f)/float(kRate));\n    const float gc=std::tan(kPi*std::clamp(cutoff,20.0f,20000.0f)/float(kRate));\n    const float r=gf/std::max(gc,1.0e-6f);\n    const float d=1.0f/std::max(q,0.1f);\n    const float den=std::sqrt((1.0f-r*r)*(1.0f-r*r)+(d*r)*(d*r));\n    const float num=shape==0?1.0f:(shape==1?r:r*r);\n    return num/std::max(den,1.0e-6f);\n}\nvoid drawFilterResponse(uint16_t curve,uint16_t grid){\n    const bool acid=current.p[P_FILTER_TYPE]>=64;\n    const int shape=acid?0:(current.p[P_FILTER_SHAPE]>84?2:current.p[P_FILTER_SHAPE]>42?1:0);\n    const bool twoStage=acid||current.p[P_FILTER_SLOPE]!=0;\n    const float cutoff=cutoffHzFor(current.p[P_CUTOFF]);\n    const float res=current.p[P_RESO]/127.0f;\n    const float q1=filterQ1(res,acid),q2=filterQ2(res,acid);\n    const int x0=15,x1=225,yTop=177,yBottom=232;\n    line(x0,205,x1,205,grid);\n    int px0=x0,py0=205;bool have=false;\n    for(int x=x0;x<=x1;++x){\n        const float t=float(x-x0)/float(x1-x0);\n        const float f=30.0f*std::pow(18000.0f/30.0f,t);\n        float mag=filterStageMagnitude(f,cutoff,q1,shape);\n        if(twoStage)mag*=filterStageMagnitude(f,cutoff,q2,shape);\n        const float db=std::clamp(20.0f*std::log10(std::max(mag,1.0e-6f)),-48.0f,18.0f);\n        const int y=int(std::lround(yTop+(18.0f-db)*(float(yBottom-yTop)/66.0f)));\n        if(have)line(px0,py0,x,y,curve);else px(x,y,curve);\n        px0=x;py0=y;have=true;\n    }\n    const float ct=std::log(cutoff/30.0f)/std::log(18000.0f/30.0f);\n    const int cx=x0+int(std::lround(std::clamp(ct,0.0f,1.0f)*(x1-x0)));\n    for(int y=yTop;y<=yBottom;y+=3)px(cx,y,grid);\n}\n\n'''
marker='''void drawScreen(){\n'''
if insert_before_draw not in s:
    if marker not in s: raise SystemExit("could not find drawScreen")
    s=s.replace(marker,insert_before_draw+marker,1)

rep(
'''    if(page==5||page==6){int yy=224;int cut=15+int(current.p[P_CUTOFF]*195/127);for(int x=15;x<225;++x){int y=x<cut?yy:yy-int(std::min(31,(x-cut)*(current.p[P_FILTER_SLOPE]?2:1))/2);px(x,y,cyan);}}''',
'''    if(page==5||page==6)drawFilterResponse(cyan,dim);''',
"response-accurate filter graph")

rep(
'''int w1=std::min(3,int(current.p[P_OSC1_WAVE]/32)),w2=std::min(3,int(current.p[P_OSC2_WAVE]/32));float a=osc(w1,voice.phase1,hz1),b=osc(w2,voice.phase2,hz2);float osc1Level=current.p[P_OSC1_LEVEL]/127.0f,osc2Level=current.osc2Level/127.0f;float s=a*osc1Level+b*osc2Level;s+=osc(1,voice.phaseSub,voice.currentHz*0.5f)*(current.p[P_SUB_LEVEL]/127.0f)*0.7f;s+=randNoise()*(current.p[P_NOISE]/127.0f)*0.25f;s+=(a*b)*(current.p[P_RING]/127.0f)*0.35f;''',
'''int w1=std::min(3,int(current.p[P_OSC1_WAVE]/32)),w2=std::min(3,int(current.p[P_OSC2_WAVE]/32));float a=osc(w1,voice.phase1,hz1),b=osc(w2,voice.phase2,hz2);float osc1Level=current.p[P_OSC1_LEVEL]/127.0f,osc2Level=current.osc2Level/127.0f;float s=a*osc1Level+b*osc2Level;float sub1=osc(1,voice.phaseSub1,hz1*0.5f),sub2=osc(1,voice.phaseSub2,hz2*0.5f);s+=sub1*(current.p[P_SUB_LEVEL]/127.0f)*0.58f;s+=sub2*(current.sub2Level/127.0f)*0.58f;s+=randNoise()*(current.p[P_NOISE]/127.0f)*0.22f;s+=(a*b)*(current.p[P_RING]/127.0f)*0.32f;''',
"per-oscillator sub DSP")

# Replace the previous single-SVF + one-pole 24 dB approximation with two TPT SVF stages.
rep(
'''void resetFilter(){voice.lp1=0.0f;voice.lp2=0.0f;voice.bp=0.0f;}\nfloat renderOne(){''',
'''void resetFilter(){voice.lp1=0.0f;voice.lp2=0.0f;voice.bp=0.0f;voice.bp2=0.0f;}\nfloat svfStage(float in,float cutoff,float q,int shape,float& ic1,float& ic2){\n    const float g=std::tan(kPi*cutoff/float(kRate));\n    const float damping=1.0f/std::max(q,0.1f);\n    const float a1=1.0f/(1.0f+g*(g+damping));\n    const float a2=g*a1,a3=g*a2;\n    const float v3=in-ic2;\n    const float band=a1*ic1+a2*v3;\n    const float low=ic2+a2*ic1+a3*v3;\n    ic1=2.0f*band-ic1;ic2=2.0f*low-ic2;\n    const float high=in-damping*band-low;\n    return shape==2?high:shape==1?band:low;\n}\nfloat renderOne(){''',
"two-stage SVF helper")

old_filter = '''    float drive=1.0f+8.0f*std::pow(current.p[P_DRIVE]/127.0f,2.0f);s=std::tanh(s*drive)/std::tanh(drive);\n    const float baseCut=cutoffHzFor(current.p[P_CUTOFF]);const float envSemis=(int(current.p[P_ENV_AMT])-64)*0.45f*voice.envMod;const float lfoSemis=l2*(current.p[P_LFO2_DEPTH]/127.0f)*12.0f;const float cutoff=std::clamp(baseCut*std::pow(2.0f,(envSemis+lfoSemis)/12.0f),30.0f,18000.0f);\n    const float g=std::tan(kPi*cutoff/float(kRate));const float resNorm=current.p[P_RESO]/127.0f;const float q=0.5f+9.5f*resNorm*resNorm;const float damping=1.0f/q;const float a1=1.0f/(1.0f+g*(g+damping));const float a2=g*a1;const float a3=g*a2;const float ic1=voice.bp;const float ic2=voice.lp1;const float v3=s-ic2;const float band=a1*ic1+a2*v3;const float low=ic2+a2*ic1+a3*v3;voice.bp=2.0f*band-ic1;voice.lp1=2.0f*low-ic2;const float high=s-damping*band-low;\n    float filtered=low;if(current.p[P_FILTER_SHAPE]>84)filtered=high;else if(current.p[P_FILTER_SHAPE]>42)filtered=band;if(current.p[P_FILTER_SLOPE]){const float alpha=1.0f-std::exp(-2.0f*kPi*cutoff/float(kRate));voice.lp2+=alpha*(filtered-voice.lp2);filtered=voice.lp2;}'''
new_filter = '''    const bool acidFilter=current.p[P_FILTER_TYPE]>=64;\n    const float driveNorm=current.p[P_DRIVE]/127.0f;const float drive=1.0f+(acidFilter?9.0f:6.0f)*driveNorm*driveNorm;s=std::tanh(s*drive)/std::tanh(drive);\n    const float baseCut=cutoffHzFor(current.p[P_CUTOFF]);const float envSemis=(int(current.p[P_ENV_AMT])-64)*0.45f*voice.envMod;const float lfoSemis=l2*(current.p[P_LFO2_DEPTH]/127.0f)*12.0f;const float cutoff=std::clamp(baseCut*std::pow(2.0f,(envSemis+lfoSemis)/12.0f),30.0f,18000.0f);\n    const float resNorm=current.p[P_RESO]/127.0f;const int shape=acidFilter?0:(current.p[P_FILTER_SHAPE]>84?2:current.p[P_FILTER_SHAPE]>42?1:0);const bool twoStage=acidFilter||current.p[P_FILTER_SLOPE]!=0;\n    if(resNorm>0.94f)s+=randNoise()*1.0e-5f*((resNorm-0.94f)/0.06f);\n    float filtered=svfStage(s,cutoff,filterQ1(resNorm,acidFilter),shape,voice.bp,voice.lp1);\n    if(twoStage)filtered=svfStage(filtered,cutoff,filterQ2(resNorm,acidFilter),shape,voice.bp2,voice.lp2);'''
rep(old_filter,new_filter,"Bass Station style two-stage filter")

if 'phaseSub,' in s or 'voice.phaseSub,' in s:
    raise SystemExit('stale single sub phase remains')
if s == orig:
    raise SystemExit("no changes made")
path.write_text(s, encoding="utf-8")
print(f"patched per-oscillator subs and filter response in {path}")
