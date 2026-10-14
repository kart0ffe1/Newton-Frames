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

rep(
'''constexpr int kDiscreteStepThreshold = 4;\nconstexpr float kPi = 3.14159265358979323846f;''',
'''constexpr int kDiscreteStepThreshold = 4;\nconstexpr uint8_t kMixerStorageVersion = 0xB2;\nconstexpr int kNoParam = -1;\nconstexpr int kVirtualOsc2Level = -2;\nconstexpr float kPi = 3.14159265358979323846f;''',
"mixer constants")

s = s.replace('P_OSC_MIX, P_SUB_LEVEL, P_NOISE, P_RING,',
              'P_OSC1_LEVEL, P_SUB_LEVEL, P_NOISE, P_RING,')

rep(
'''struct Preset {\n    uint8_t p[kParamCount];\n    char name[16];\n};''',
'''struct Preset {\n    uint8_t p[kParamCount];\n    // Keep the struct exactly 48 bytes so existing flash.bin files remain readable.\n    // The final two bytes used to be unused tail bytes of name[16].\n    char name[14];\n    uint8_t mixerVersion = 0;\n    uint8_t osc2Level = 64;\n};\nstatic_assert(sizeof(Preset) == 48, "Preset flash layout must stay backward compatible");''',
"preset layout")

old_defaults = '''void defaults(Preset& p,int i){ std::fill(std::begin(p.p),std::end(p.p),64); p.p[P_OSC1_WAVE]=0;p.p[P_OSC1_RANGE]=64;p.p[P_OSC1_COARSE]=64;p.p[P_OSC1_FINE]=64;p.p[P_OSC2_WAVE]=0;p.p[P_OSC2_RANGE]=64;p.p[P_OSC2_COARSE]=64;p.p[P_OSC2_FINE]=70;p.p[P_OSC_MIX]=64;p.p[P_SUB_LEVEL]=60;p.p[P_NOISE]=4;p.p[P_RING]=0;p.p[P_CUTOFF]=78;p.p[P_RESO]=35;p.p[P_ENV_AMT]=78;p.p[P_DRIVE]=18;p.p[P_AMP_A]=3;p.p[P_AMP_D]=42;p.p[P_AMP_S]=105;p.p[P_AMP_R]=28;p.p[P_MOD_A]=2;p.p[P_MOD_D]=45;p.p[P_MOD_S]=8;p.p[P_MOD_R]=25;p.p[P_LFO1_RATE]=35;p.p[P_LFO1_DEPTH]=8;p.p[P_LFO2_RATE]=25;p.p[P_LFO2_DEPTH]=10;p.p[P_GLIDE]=0;p.p[P_FILTER_TYPE]=0;p.p[P_FILTER_SHAPE]=0;p.p[P_FILTER_SLOPE]=127; std::snprintf(p.name,sizeof(p.name),"BSFM %03d",i+1); }\nvoid loadPreset(){ current=flash[size_t(presetIndex)]; }\nvoid savePreset(){ flash[size_t(presetIndex)]=current; ++flashWrites; }'''
new_defaults = '''void defaults(Preset& p,int i){ std::fill(std::begin(p.p),std::end(p.p),64); p.p[P_OSC1_WAVE]=0;p.p[P_OSC1_RANGE]=64;p.p[P_OSC1_COARSE]=64;p.p[P_OSC1_FINE]=64;p.p[P_OSC2_WAVE]=0;p.p[P_OSC2_RANGE]=64;p.p[P_OSC2_COARSE]=64;p.p[P_OSC2_FINE]=70;p.p[P_OSC1_LEVEL]=63;p.p[P_SUB_LEVEL]=60;p.p[P_NOISE]=4;p.p[P_RING]=0;p.p[P_CUTOFF]=78;p.p[P_RESO]=35;p.p[P_ENV_AMT]=78;p.p[P_DRIVE]=18;p.p[P_AMP_A]=3;p.p[P_AMP_D]=42;p.p[P_AMP_S]=105;p.p[P_AMP_R]=28;p.p[P_MOD_A]=2;p.p[P_MOD_D]=45;p.p[P_MOD_S]=8;p.p[P_MOD_R]=25;p.p[P_LFO1_RATE]=35;p.p[P_LFO1_DEPTH]=8;p.p[P_LFO2_RATE]=25;p.p[P_LFO2_DEPTH]=10;p.p[P_GLIDE]=0;p.p[P_FILTER_TYPE]=0;p.p[P_FILTER_SHAPE]=0;p.p[P_FILTER_SLOPE]=127;p.mixerVersion=kMixerStorageVersion;p.osc2Level=64; std::snprintf(p.name,sizeof(p.name),"BSFM %03d",i+1); }\nvoid migrateMixerStorage(Preset& p){\n    if(p.mixerVersion==kMixerStorageVersion)return;\n    const uint8_t oldCrossfade=p.p[P_OSC1_LEVEL];\n    p.p[P_OSC1_LEVEL]=uint8_t(127u-oldCrossfade);\n    p.osc2Level=oldCrossfade;\n    p.mixerVersion=kMixerStorageVersion;\n}\nvoid loadPreset(){current=flash[size_t(presetIndex)];migrateMixerStorage(current);}\nvoid savePreset(){current.mixerVersion=kMixerStorageVersion;flash[size_t(presetIndex)]=current;++flashWrites;}'''
rep(old_defaults, new_defaults, "defaults and migration")

rep(
'''int subPageCount(int c){ switch(c){case C_OSC:return 2;case C_FILTER:return 2;case C_ENV:return 2;default:return 1;} }''',
'''int subPageCount(int c){ switch(c){case C_OSC:return 2;case C_MIXER:return 2;case C_FILTER:return 2;case C_ENV:return 2;default:return 1;} }''',
"mixer subpage count")

rep(
'''    static const int firstPage[C_COUNT]={0,1,3,4,6,8};''',
'''    static const int firstPage[C_COUNT]={0,1,3,5,7,9};''',
"category first pages")

old_knobs = '''        case 0:{static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE};return a[k];}\n        case 1:{static int a[4]={P_OSC1_WAVE,P_OSC1_RANGE,P_OSC1_COARSE,P_OSC1_FINE};return a[k];}\n        case 2:{static int a[4]={P_OSC2_WAVE,P_OSC2_RANGE,P_OSC2_COARSE,P_OSC2_FINE};return a[k];}\n        case 3:{static int a[4]={P_OSC_MIX,P_SUB_LEVEL,P_NOISE,P_RING};return a[k];}\n        case 4:{static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE};return a[k];}\n        case 5:{static int a[4]={P_FILTER_TYPE,P_FILTER_SHAPE,P_FILTER_SLOPE,P_GLIDE};return a[k];}\n        case 6:{static int a[4]={P_AMP_A,P_AMP_D,P_AMP_S,P_AMP_R};return a[k];}\n        case 7:{static int a[4]={P_MOD_A,P_MOD_D,P_MOD_S,P_MOD_R};return a[k];}\n        case 8:{static int a[4]={P_LFO1_RATE,P_LFO1_DEPTH,P_LFO2_RATE,P_LFO2_DEPTH};return a[k];}'''
new_knobs = '''        case 0:{static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE};return a[k];}\n        case 1:{static int a[4]={P_OSC1_WAVE,P_OSC1_RANGE,P_OSC1_COARSE,P_OSC1_FINE};return a[k];}\n        case 2:{static int a[4]={P_OSC2_WAVE,P_OSC2_RANGE,P_OSC2_COARSE,P_OSC2_FINE};return a[k];}\n        case 3:{static int a[4]={P_OSC1_LEVEL,kVirtualOsc2Level,P_SUB_LEVEL,P_NOISE};return a[k];}\n        case 4:{static int a[4]={P_RING,kNoParam,kNoParam,kNoParam};return a[k];}\n        case 5:{static int a[4]={P_CUTOFF,P_RESO,P_ENV_AMT,P_DRIVE};return a[k];}\n        case 6:{static int a[4]={P_FILTER_TYPE,P_FILTER_SHAPE,P_FILTER_SLOPE,P_GLIDE};return a[k];}\n        case 7:{static int a[4]={P_AMP_A,P_AMP_D,P_AMP_S,P_AMP_R};return a[k];}\n        case 8:{static int a[4]={P_MOD_A,P_MOD_D,P_MOD_S,P_MOD_R};return a[k];}\n        case 9:{static int a[4]={P_LFO1_RATE,P_LFO1_DEPTH,P_LFO2_RATE,P_LFO2_DEPTH};return a[k];}'''
rep(old_knobs, new_knobs, "mixer page parameter mapping")

rep(
'''const char* pageName(){ static const char* n[]={"HOME","OSC 1","OSC 2","MIXER","FILTER","FILTER 2","AMP ENV","MOD ENV","LFO"}; return n[std::clamp(page,0,8)]; }\nconst char* paramName(int p){ static const char* n[kParamCount]={"WAVE","RANGE","COARSE","FINE","WAVE","RANGE","COARSE","FINE","OSC MIX","SUB","NOISE","RING","CUTOFF","RESO","ENV AMT","DRIVE","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","LFO1 RATE","LFO1 DEP","LFO2 RATE","LFO2 DEP","GLIDE","TYPE","SHAPE","SLOPE"}; return n[p]; }''',
'''const char* pageName(){ static const char* n[]={"HOME","OSC 1","OSC 2","MIXER 1","MIXER 2","FILTER","FILTER 2","AMP ENV","MOD ENV","LFO"}; return n[std::clamp(page,0,9)]; }\nconst char* paramName(int p){\n    if(p==kVirtualOsc2Level)return "OSC2";\n    if(p==kNoParam)return "";\n    static const char* n[kParamCount]={"WAVE","RANGE","COARSE","FINE","WAVE","RANGE","COARSE","FINE","OSC1","SUB","NOISE","RING","CUTOFF","RESO","ENV AMT","DRIVE","ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE","LFO1 RATE","LFO1 DEP","LFO2 RATE","LFO2 DEP","GLIDE","TYPE","SHAPE","SLOPE"}; return n[p];\n}\nuint8_t paramRawValue(int p){if(p==kVirtualOsc2Level)return current.osc2Level;if(p>=0&&p<kParamCount)return current.p[p];return 0;}''',
"page and parameter names")

rep(
'''void formatParamValue(int p,char* b,size_t n){\n    const uint8_t v=current.p[p];''',
'''void formatParamValue(int p,char* b,size_t n){\n    if(p==kNoParam){if(n)b[0]=0;return;}\n    const uint8_t v=paramRawValue(p);\n    if(p==kVirtualOsc2Level){std::snprintf(b,n,"%d%%",percent(v));return;}''',
"virtual parameter formatting")

s = s.replace('p==P_AMP_S||p==P_MOD_S||p==P_OSC_MIX||',
              'p==P_AMP_S||p==P_MOD_S||p==P_OSC1_LEVEL||')

rep(
'''void adjustParam(int encoder,int p,int delta){\n    if(encoder<0||encoder>=4||delta==0)return;''',
'''void adjustParam(int encoder,int p,int delta){\n    if(encoder<0||encoder>=4||delta==0||p==kNoParam)return;\n    if(p==kVirtualOsc2Level){current.osc2Level=uint8_t(std::clamp(int(current.osc2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}''',
"virtual parameter editing")

rep(
'''        int p=knobParam(k),x=(k%2)*116+10,y=(k/2)*55+68;char value[16]{};\n        text(x,y,paramName(p),dim,1);formatParamValue(p,value,sizeof(value));text(x,y+15,value,pale,2);\n        int w=int(current.p[p]*92/127);fillRect(x,y+36,94,7,deep);fillRect(x,y+36,w,7,blue);''',
'''        int p=knobParam(k),x=(k%2)*116+10,y=(k/2)*55+68;char value[16]{};\n        if(p==kNoParam)continue;\n        text(x,y,paramName(p),dim,1);formatParamValue(p,value,sizeof(value));text(x,y+15,value,pale,2);\n        int w=int(paramRawValue(p)*92/127);fillRect(x,y+36,94,7,deep);fillRect(x,y+36,w,7,blue);''',
"mixer display values")

s = s.replace('if(page==6||page==7){\n        int base=page==6?P_AMP_A:P_MOD_A;',
              'if(page==7||page==8){\n        int base=page==7?P_AMP_A:P_MOD_A;')
s = s.replace('if(page==4||page==5){int yy=224;',
              'if(page==5||page==6){int yy=224;')

old_render = '''int w1=std::min(3,int(current.p[P_OSC1_WAVE]/32)),w2=std::min(3,int(current.p[P_OSC2_WAVE]/32));float a=osc(w1,voice.phase1,hz1),b=osc(w2,voice.phase2,hz2);float mix=current.p[P_OSC_MIX]/127.0f;float s=a*(1-mix)+b*mix;s+=osc(1,voice.phaseSub,voice.currentHz*0.5f)*(current.p[P_SUB_LEVEL]/127.0f)*0.7f;'''
new_render = '''int w1=std::min(3,int(current.p[P_OSC1_WAVE]/32)),w2=std::min(3,int(current.p[P_OSC2_WAVE]/32));float a=osc(w1,voice.phase1,hz1),b=osc(w2,voice.phase2,hz2);float osc1Level=current.p[P_OSC1_LEVEL]/127.0f,osc2Level=current.osc2Level/127.0f;float s=a*osc1Level+b*osc2Level;s+=osc(1,voice.phaseSub,voice.currentHz*0.5f)*(current.p[P_SUB_LEVEL]/127.0f)*0.7f;'''
rep(old_render, new_render, "separate oscillator DSP levels")

if 'P_OSC_MIX' in s:
    raise SystemExit('stale P_OSC_MIX reference remains')

if s == orig:
    raise SystemExit("no changes made")
path.write_text(s, encoding="utf-8")
print(f"patched separate BSII mixer levels in {path}")
