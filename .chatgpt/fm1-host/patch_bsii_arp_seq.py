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

# Virtual controls live outside the compact 32-byte synth parameter array.
rep(
'''constexpr int kVirtualSub2Level = -3;\nconstexpr float kPi = 3.14159265358979323846f;''',
'''constexpr int kVirtualSub2Level = -3;\nconstexpr int kVirtualArpOn = -10;\nconstexpr int kVirtualArpTempo = -11;\nconstexpr int kVirtualArpMode = -12;\nconstexpr int kVirtualArpOctaves = -13;\nconstexpr int kVirtualArpRhythm = -14;\nconstexpr int kVirtualArpSwing = -15;\nconstexpr int kVirtualArpLatch = -16;\nconstexpr int kVirtualSeqSlot = -20;\nconstexpr int kVirtualSeqLength = -21;\nconstexpr int kVirtualSeqRhythm = -22;\nconstexpr int kVirtualSeqTempo = -23;\nconstexpr int kVirtualSeqStep = -24;\nconstexpr int kVirtualSeqNote = -25;\nconstexpr int kVirtualSeqType = -26;\nconstexpr int kVirtualSeqRetrig = -27;\nconstexpr int kSeqSlots = 4;\nconstexpr int kSeqSteps = 32;\nconstexpr float kPi = 3.14159265358979323846f;''',
"arp seq virtual constants")

rep(
'''enum Category { C_HOME, C_OSC, C_MIXER, C_FILTER, C_ENV, C_LFO, C_COUNT };''',
'''enum Category { C_HOME, C_OSC, C_MIXER, C_FILTER, C_ENV, C_LFO, C_ARP, C_SEQ, C_COUNT };''',
"arp seq categories")

rep(
'''std::array<int,4> discreteParam{{-1,-1,-1,-1}};''',
'''std::array<int,4> discreteParam{{-1,-1,-1,-1}};\n\n// Bass Station II style arp / sequencer performance state. Tempo, rhythm and\n// swing are shared by arp and sequencer, matching the hardware control section.\nbool arpEnabled=false, arpLatch=false;\nint arpTempo=120, arpMode=0, arpOctaves=1, arpRhythm=1, arpSwing=50;\nstd::array<uint8_t,128> rawHeld{}, arpHeld{};\nstd::array<int,128> playedOrder{};\nint playedOrderCount=0;\ndouble arpSamplesUntil=0.0;\nint arpGridStep=0, arpNoteIndex=0, arpGateCountdown=0, arpCurrentNote=-1;\n\nbool seqPlaying=false, seqRecording=false, seqRetrig=false;\nint seqSlot=0, seqEditStep=0, seqPlayStep=0, seqGridStep=0;\nint seqRecordPos=0, seqRecordRoot=60, seqTransposeRoot=60;\nbool seqRecordRootSet=false;\ndouble seqSamplesUntil=0.0;\nint seqGateCountdown=0, seqCurrentNote=-1;''',
"arp seq runtime state")

# Reserve the unused tail bytes of preset names as persistent four-by-32 sequence memory.
# This keeps nor_size() exactly unchanged, so existing flash.bin files remain compatible.
rep(
'''void savePreset(){current.mixerVersion=kMixerStorageVersion;flash[size_t(presetIndex)]=current;++flashWrites;}''',
'''int seqFlashIndex(int slot,int step){return std::clamp(slot,0,kSeqSlots-1)*kSeqSteps+std::clamp(step,0,kSeqSteps-1);}\nuint8_t seqPitchCode(int slot,int step){return uint8_t(flash[size_t(seqFlashIndex(slot,step))].name[11]);}\nint seqStepTypeStored(int slot,int step){const uint8_t code=seqPitchCode(slot,step);if(code==0)return 2;return std::clamp(int(uint8_t(flash[size_t(seqFlashIndex(slot,step))].name[12])),0,2);}\nint seqOffsetStored(int slot,int step){const int code=int(seqPitchCode(slot,step));return code>=16&&code<=111?code-64:0;}\nint seqLengthStored(int slot){const int n=int(uint8_t(flash[size_t(std::clamp(slot,0,kSeqSlots-1))].name[10]));return n>=1&&n<=kSeqSteps?n:16;}\nvoid setSeqLengthStored(int slot,int length){slot=std::clamp(slot,0,kSeqSlots-1);length=std::clamp(length,1,kSeqSteps);flash[size_t(slot)].name[10]=char(length);++flashWrites;}\nvoid writeSeqStep(int slot,int step,int offset,int type){const int i=seqFlashIndex(slot,step);offset=std::clamp(offset,-48,47);flash[size_t(i)].name[11]=char(offset+64);flash[size_t(i)].name[12]=char(std::clamp(type,0,2));++flashWrites;}\nvoid clearSequence(int slot){slot=std::clamp(slot,0,kSeqSlots-1);for(int st=0;st<kSeqSteps;++st){const int i=seqFlashIndex(slot,st);flash[size_t(i)].name[11]=0;flash[size_t(i)].name[12]=2;}flash[size_t(slot)].name[10]=1;++flashWrites;}\nvoid savePreset(){\n    current.mixerVersion=kMixerStorageVersion;\n    // Do not overwrite the hidden global sequence bytes that share name padding.\n    const char m10=flash[size_t(presetIndex)].name[10],m11=flash[size_t(presetIndex)].name[11],m12=flash[size_t(presetIndex)].name[12];\n    flash[size_t(presetIndex)]=current;\n    flash[size_t(presetIndex)].name[10]=m10;flash[size_t(presetIndex)].name[11]=m11;flash[size_t(presetIndex)].name[12]=m12;\n    ++flashWrites;\n}''',
"persistent sequence storage")

rep(
'''int subPageCount(int c){ switch(c){case C_OSC:return 2;case C_MIXER:return 2;case C_FILTER:return 2;case C_ENV:return 2;default:return 1;} }''',
'''int subPageCount(int c){ switch(c){case C_OSC:return 2;case C_MIXER:return 2;case C_FILTER:return 2;case C_ENV:return 2;case C_ARP:return 2;case C_SEQ:return 2;default:return 1;} }''',
"arp seq subpages")
rep(
'''    static const int firstPage[C_COUNT]={0,1,3,5,7,9};''',
'''    static const int firstPage[C_COUNT]={0,1,3,5,7,9,10,12};''',
"arp seq first pages")

# Add the two arp pages and two sequencer pages.
rep(
'''        case 9:{static int a[4]={P_LFO1_RATE,P_LFO1_DEPTH,P_LFO2_RATE,P_LFO2_DEPTH};return a[k];}\n        default:return P_CUTOFF;''',
'''        case 9:{static int a[4]={P_LFO1_RATE,P_LFO1_DEPTH,P_LFO2_RATE,P_LFO2_DEPTH};return a[k];}\n        case 10:{static int a[4]={kVirtualArpOn,kVirtualArpTempo,kVirtualArpMode,kVirtualArpOctaves};return a[k];}\n        case 11:{static int a[4]={kVirtualArpRhythm,kVirtualArpSwing,kVirtualArpLatch,kNoParam};return a[k];}\n        case 12:{static int a[4]={kVirtualSeqSlot,kVirtualSeqLength,kVirtualSeqRhythm,kVirtualSeqTempo};return a[k];}\n        case 13:{static int a[4]={kVirtualSeqStep,kVirtualSeqNote,kVirtualSeqType,kVirtualSeqRetrig};return a[k];}\n        default:return P_CUTOFF;''',
"arp seq knob pages")

rep(
'''const char* pageName(){ static const char* n[]={"HOME","OSC 1","OSC 2","MIXER 1","MIXER 2","FILTER","FILTER 2","AMP ENV","MOD ENV","LFO"}; return n[std::clamp(page,0,9)]; }''',
'''const char* pageName(){ static const char* n[]={"HOME","OSC 1","OSC 2","MIXER 1","MIXER 2","FILTER","FILTER 2","AMP ENV","MOD ENV","LFO","ARP 1","ARP 2","SEQ 1","SEQ 2"}; return n[std::clamp(page,0,13)]; }''',
"arp seq page names")

rep(
'''    if(p==kVirtualSub2Level)return "SUB2";\n    if(p==kNoParam)return "";''',
'''    if(p==kVirtualSub2Level)return "SUB2";\n    if(p==kVirtualArpOn)return "ON";\n    if(p==kVirtualArpTempo)return "TEMPO";\n    if(p==kVirtualArpMode)return "MODE";\n    if(p==kVirtualArpOctaves)return "OCTAVES";\n    if(p==kVirtualArpRhythm||p==kVirtualSeqRhythm)return "RHYTHM";\n    if(p==kVirtualArpSwing)return "SWING";\n    if(p==kVirtualArpLatch)return "LATCH";\n    if(p==kVirtualSeqSlot)return "SEQ";\n    if(p==kVirtualSeqLength)return "LENGTH";\n    if(p==kVirtualSeqTempo)return "TEMPO";\n    if(p==kVirtualSeqStep)return "STEP";\n    if(p==kVirtualSeqNote)return "NOTE";\n    if(p==kVirtualSeqType)return "TYPE";\n    if(p==kVirtualSeqRetrig)return "RETRIG";\n    if(p==kNoParam)return "";''',
"arp seq parameter names")

rep(
'''uint8_t paramRawValue(int p){if(p==kVirtualOsc2Level)return current.osc2Level;if(p==kVirtualSub2Level)return current.sub2Level;if(p>=0&&p<kParamCount)return current.p[p];return 0;}''',
'''uint8_t paramRawValue(int p){\n    if(p==kVirtualOsc2Level)return current.osc2Level;if(p==kVirtualSub2Level)return current.sub2Level;\n    if(p==kVirtualArpOn)return arpEnabled?127:0;if(p==kVirtualArpTempo||p==kVirtualSeqTempo)return uint8_t(std::clamp((arpTempo-40)*127/200,0,127));\n    if(p==kVirtualArpMode)return uint8_t(arpMode*127/5);if(p==kVirtualArpOctaves)return uint8_t((arpOctaves-1)*127/3);\n    if(p==kVirtualArpRhythm||p==kVirtualSeqRhythm)return uint8_t((arpRhythm-1)*127/31);if(p==kVirtualArpSwing)return uint8_t((arpSwing-1)*127/98);\n    if(p==kVirtualArpLatch)return arpLatch?127:0;if(p==kVirtualSeqSlot)return uint8_t(seqSlot*127/3);\n    if(p==kVirtualSeqLength)return uint8_t((seqLengthStored(seqSlot)-1)*127/31);if(p==kVirtualSeqStep)return uint8_t(seqEditStep*127/31);\n    if(p==kVirtualSeqNote)return uint8_t(std::clamp((seqOffsetStored(seqSlot,seqEditStep)+48)*127/95,0,127));\n    if(p==kVirtualSeqType)return uint8_t(seqStepTypeStored(seqSlot,seqEditStep)*127/2);if(p==kVirtualSeqRetrig)return seqRetrig?127:0;\n    if(p==P_FILTER_SHAPE&&current.p[P_FILTER_TYPE]>=64)return 0;if(p==P_FILTER_SLOPE&&current.p[P_FILTER_TYPE]>=64)return 127;\n    if(p>=0&&p<kParamCount)return current.p[p];return 0;\n}''',
"arp seq raw display values")

# Friendly parameter values instead of MIDI values. Acid shape/slope show their actual fixed state.
rep(
'''    if(p==kVirtualOsc2Level||p==kVirtualSub2Level){std::snprintf(b,n,"%d%%",percent(v));return;}''',
'''    if(p==kVirtualOsc2Level||p==kVirtualSub2Level){std::snprintf(b,n,"%d%%",percent(v));return;}\n    if(p==kVirtualArpOn){std::snprintf(b,n,"%s",arpEnabled?"ON":"OFF");return;}\n    if(p==kVirtualArpTempo||p==kVirtualSeqTempo){std::snprintf(b,n,"%dBPM",arpTempo);return;}\n    if(p==kVirtualArpMode){static const char* m[]={"UP","DWN","UDN","UD2","PLY","RND"};std::snprintf(b,n,"%s",m[std::clamp(arpMode,0,5)]);return;}\n    if(p==kVirtualArpOctaves){std::snprintf(b,n,"%dOCT",arpOctaves);return;}\n    if(p==kVirtualArpRhythm||p==kVirtualSeqRhythm){std::snprintf(b,n,"R%02d",arpRhythm);return;}\n    if(p==kVirtualArpSwing){std::snprintf(b,n,"%d%%",arpSwing);return;}\n    if(p==kVirtualArpLatch){std::snprintf(b,n,"%s",arpLatch?"ON":"OFF");return;}\n    if(p==kVirtualSeqSlot){std::snprintf(b,n,"SEQ%d",seqSlot+1);return;}\n    if(p==kVirtualSeqLength){std::snprintf(b,n,"%d",seqLengthStored(seqSlot));return;}\n    if(p==kVirtualSeqStep){std::snprintf(b,n,"S%02d",seqEditStep+1);return;}\n    if(p==kVirtualSeqNote){if(seqPitchCode(seqSlot,seqEditStep)==0)std::snprintf(b,n,"---");else std::snprintf(b,n,"%+dST",seqOffsetStored(seqSlot,seqEditStep));return;}\n    if(p==kVirtualSeqType){static const char* t[]={"NOTE","LEG","REST"};std::snprintf(b,n,"%s",t[seqStepTypeStored(seqSlot,seqEditStep)]);return;}\n    if(p==kVirtualSeqRetrig){std::snprintf(b,n,"%s",seqRetrig?"ON":"OFF");return;}''',
"arp seq formatted values")

rep(
'''    if(p==P_FILTER_SHAPE){std::snprintf(b,n,"%s",v>84?"HP":v>42?"BP":"LP");return;}\n    if(p==P_FILTER_SLOPE){std::snprintf(b,n,"%s",v?"24DB":"12DB");return;}''',
'''    if(p==P_FILTER_SHAPE){if(current.p[P_FILTER_TYPE]>=64)std::snprintf(b,n,"LP");else std::snprintf(b,n,"%s",v>84?"HP":v>42?"BP":"LP");return;}\n    if(p==P_FILTER_SLOPE){if(current.p[P_FILTER_TYPE]>=64)std::snprintf(b,n,"24DB");else std::snprintf(b,n,"%s",v?"24DB":"12DB");return;}''',
"acid fixed filter display")

# Common detent helper for switch-like virtual controls.
insert_before_adjust='''int consumeVirtualDetents(int encoder,int p,int delta){\n    if(discreteParam[size_t(encoder)]!=p){discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;}\n    int& accum=discreteAccum[size_t(encoder)];accum+=delta;int steps=0;\n    while(accum>=kDiscreteStepThreshold){++steps;accum-=kDiscreteStepThreshold;}\n    while(accum<=-kDiscreteStepThreshold){--steps;accum+=kDiscreteStepThreshold;}\n    return steps;\n}\nvoid resetClockPositions(){arpSamplesUntil=0.0;arpGridStep=0;arpNoteIndex=0;seqSamplesUntil=0.0;seqGridStep=0;seqPlayStep=0;}\n\n'''
marker='''void adjustParam(int encoder,int p,int delta){\n'''
if insert_before_adjust not in s:
    if marker not in s: raise SystemExit("could not find adjustParam")
    s=s.replace(marker,insert_before_adjust+marker,1)

# Handle all arp/seq page controls, then fall through to synth parameters.
rep(
'''void adjustParam(int encoder,int p,int delta){\n    if(encoder<0||encoder>=4||delta==0||p==kNoParam)return;\n    if(p==kVirtualOsc2Level){current.osc2Level=uint8_t(std::clamp(int(current.osc2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualSub2Level){current.sub2Level=uint8_t(std::clamp(int(current.sub2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}''',
'''void adjustParam(int encoder,int p,int delta){\n    if(encoder<0||encoder>=4||delta==0||p==kNoParam)return;\n    if(p==kVirtualOsc2Level){current.osc2Level=uint8_t(std::clamp(int(current.osc2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualSub2Level){current.sub2Level=uint8_t(std::clamp(int(current.sub2Level)+delta,0,127));discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualArpOn){int st=consumeVirtualDetents(encoder,p,delta);if(st){arpEnabled=st>0;seqPlaying=false;seqRecording=false;resetClockPositions();}return;}\n    if(p==kVirtualArpTempo||p==kVirtualSeqTempo){arpTempo=std::clamp(arpTempo+delta,40,240);discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualArpMode){int st=consumeVirtualDetents(encoder,p,delta);if(st)arpMode=std::clamp(arpMode+st,0,5);return;}\n    if(p==kVirtualArpOctaves){int st=consumeVirtualDetents(encoder,p,delta);if(st)arpOctaves=std::clamp(arpOctaves+st,1,4);return;}\n    if(p==kVirtualArpRhythm||p==kVirtualSeqRhythm){int st=consumeVirtualDetents(encoder,p,delta);if(st)arpRhythm=std::clamp(arpRhythm+st,1,32);return;}\n    if(p==kVirtualArpSwing){arpSwing=std::clamp(arpSwing+delta,1,99);discreteParam[size_t(encoder)]=p;discreteAccum[size_t(encoder)]=0;return;}\n    if(p==kVirtualArpLatch){int st=consumeVirtualDetents(encoder,p,delta);if(st)arpLatch=st>0;return;}\n    if(p==kVirtualSeqSlot){int st=consumeVirtualDetents(encoder,p,delta);if(st){seqSlot=std::clamp(seqSlot+st,0,3);seqEditStep=0;seqPlayStep=0;}return;}\n    if(p==kVirtualSeqLength){setSeqLengthStored(seqSlot,seqLengthStored(seqSlot)+delta);return;}\n    if(p==kVirtualSeqStep){seqEditStep=std::clamp(seqEditStep+delta,0,31);return;}\n    if(p==kVirtualSeqNote){int off=seqPitchCode(seqSlot,seqEditStep)==0?0:seqOffsetStored(seqSlot,seqEditStep);int type=seqPitchCode(seqSlot,seqEditStep)==0?0:seqStepTypeStored(seqSlot,seqEditStep);writeSeqStep(seqSlot,seqEditStep,off+delta,type);return;}\n    if(p==kVirtualSeqType){int st=consumeVirtualDetents(encoder,p,delta);if(st){int type=seqStepTypeStored(seqSlot,seqEditStep);int off=seqPitchCode(seqSlot,seqEditStep)==0?0:seqOffsetStored(seqSlot,seqEditStep);type=std::clamp(type+st,0,2);writeSeqStep(seqSlot,seqEditStep,off,type);}return;}\n    if(p==kVirtualSeqRetrig){int st=consumeVirtualDetents(encoder,p,delta);if(st)seqRetrig=st>0;return;}\n    // On the real BSII Acid is a fixed LP24 filter: Shape and Slope are inoperative.\n    if(current.p[P_FILTER_TYPE]>=64&&(p==P_FILTER_SHAPE||p==P_FILTER_SLOPE))return;''',
"arp seq parameter editing")

# Small rhythm/sequence visualization in the lower display area.
insert_before_draw='''bool rhythmHit(int pattern,int step){\n    pattern=std::clamp(pattern,1,32);step=((step%32)+32)%32;\n    if(pattern==1)return (step%4)==0;\n    if(pattern==32)return true;\n    const int hits=8+((pattern-1)*24)/31;\n    const int rotated=(step+(pattern*3)%32)%32;\n    return ((rotated*hits)%32)<hits;\n}\nvoid drawRhythmStrip(uint16_t on,uint16_t off){\n    for(int i=0;i<32;++i){const int x=8+i*7;const bool hit=rhythmHit(arpRhythm,i);fillRect(x,218,5,8,hit?on:off);}\n}\nvoid drawSequenceStrip(uint16_t on,uint16_t off,uint16_t rest){\n    const int len=seqLengthStored(seqSlot);\n    for(int i=0;i<32;++i){const int x=8+i*7;uint16_t c=off;if(i<len){const int type=seqStepTypeStored(seqSlot,i);c=type==2?rest:on;}fillRect(x,218,5,8,c);if(i==seqEditStep)line(x,229,x+4,229,on);}\n}\n\n'''
marker='''void drawScreen(){\n'''
if insert_before_draw not in s:
    if marker not in s: raise SystemExit("could not find drawScreen for rhythm strip")
    s=s.replace(marker,insert_before_draw+marker,1)

# Add arp/seq strips after the waveform graph.
rep(
'''        for(int x=15;x<225;++x){float t=float(x-15)/35.0f;float frac=std::fmod(t,1.0f);float yv=wave==0?2*frac-1:wave==1?(frac<.5f?1.0f:-1.0f):wave==2?2*std::fabs(2*frac-1)-1:std::sin(2*kPi*t);int y=210-int(yv*15);if(have)line(prevX,prevY,x,y,cyan);else px(x,y,cyan);prevX=x;prevY=y;have=true;}\n    }\n}''',
'''        for(int x=15;x<225;++x){float t=float(x-15)/35.0f;float frac=std::fmod(t,1.0f);float yv=wave==0?2*frac-1:wave==1?(frac<.5f?1.0f:-1.0f):wave==2?2*std::fabs(2*frac-1)-1:std::sin(2*kPi*t);int y=210-int(yv*15);if(have)line(prevX,prevY,x,y,cyan);else px(x,y,cyan);prevX=x;prevY=y;have=true;}\n    }\n    if(page==10||page==11)drawRhythmStrip(cyan,deep);\n    if(page==12||page==13)drawSequenceStrip(cyan,deep,dim);\n}''',
"arp seq display strips")

# Split performance note handling from the synth voice itself.
rep(
'''void noteOn(int n,int vel){voice.note=n;voice.velocity=vel;voice.targetHz=midiHz(n);if(!voice.gate||current.p[P_GLIDE]==0)voice.currentHz=voice.targetHz;voice.gate=true;voice.ampStage=1;voice.modStage=1;}\nvoid noteOff(int n){if(n==voice.note){voice.gate=false;voice.ampStage=4;voice.modStage=4;}}''',
'''void voiceNoteOn(int n,int vel){voice.note=n;voice.velocity=vel;voice.targetHz=midiHz(n);if(!voice.gate||current.p[P_GLIDE]==0)voice.currentHz=voice.targetHz;voice.gate=true;voice.ampStage=1;voice.modStage=1;}\nvoid voiceNoteOff(int n){if(n==voice.note){voice.gate=false;voice.ampStage=4;voice.modStage=4;}}\nvoid voiceLegatoTo(int n){voice.note=n;voice.targetHz=midiHz(n);if(current.p[P_GLIDE]==0)voice.currentHz=voice.targetHz;voice.gate=true;}\nint rawHeldCount(){int c=0;for(auto v:rawHeld)c+=v?1:0;return c;}\nvoid removePlayedOrder(int n){for(int i=0;i<playedOrderCount;++i)if(playedOrder[size_t(i)]==n){for(int j=i;j+1<playedOrderCount;++j)playedOrder[size_t(j)]=playedOrder[size_t(j+1)];--playedOrderCount;return;}}\nvoid addPlayedOrder(int n){removePlayedOrder(n);if(playedOrderCount<128)playedOrder[size_t(playedOrderCount++)]=n;}\nvoid inputNoteOn(int n,int vel){\n    n=std::clamp(n,0,127);const bool newChord=rawHeldCount()==0;rawHeld[size_t(n)]=1;addPlayedOrder(n);\n    if(seqRecording){if(seqRecordPos<32){if(!seqRecordRootSet){seqRecordRoot=n;seqRecordRootSet=true;}writeSeqStep(seqSlot,seqRecordPos,n-seqRecordRoot,0);++seqRecordPos;setSeqLengthStored(seqSlot,std::max(1,seqRecordPos));seqEditStep=std::min(31,seqRecordPos);if(seqRecordPos>=32)seqRecording=false;}voiceNoteOn(n,vel);drawScreen();return;}\n    if(seqPlaying){seqTransposeRoot=n;return;}\n    if(arpEnabled){if(arpLatch&&newChord){arpHeld.fill(0);playedOrderCount=0;addPlayedOrder(n);}arpHeld[size_t(n)]=1;return;}\n    voiceNoteOn(n,vel);\n}\nvoid inputNoteOff(int n){\n    n=std::clamp(n,0,127);rawHeld[size_t(n)]=0;\n    if(seqRecording){voiceNoteOff(n);return;}\n    if(seqPlaying)return;\n    if(arpEnabled){if(!arpLatch){arpHeld[size_t(n)]=0;removePlayedOrder(n);}return;}\n    removePlayedOrder(n);voiceNoteOff(n);\n}\nvoid stopClockVoice(){if(arpCurrentNote>=0)voiceNoteOff(arpCurrentNote);if(seqCurrentNote>=0)voiceNoteOff(seqCurrentNote);arpCurrentNote=seqCurrentNote=-1;arpGateCountdown=seqGateCountdown=0;}\nvoid toggleSeqPlay(){seqPlaying=!seqPlaying;seqRecording=false;arpEnabled=false;stopClockVoice();resetClockPositions();if(seqPlaying&&rawHeldCount()>0){for(int n=0;n<128;++n)if(rawHeld[size_t(n)]){seqTransposeRoot=n;break;}}drawScreen();}\nvoid toggleSeqRecord(){if(!seqRecording){seqRecording=true;seqPlaying=false;arpEnabled=false;stopClockVoice();clearSequence(seqSlot);seqRecordPos=0;seqEditStep=0;seqRecordRootSet=false;}else{seqRecording=false;}drawScreen();}''',
"performance note routing")

# Buttons: ARP/SEQ are menu shortcuts; PLAY/REC operate the sequencer.
rep(
'''    if(pressed&(1u<<7))savePreset();\n    if(pressed&(1u<<12))voice.note-=12,voice.targetHz=midiHz(voice.note);''',
'''    if(pressed&(1u<<7))savePreset();\n    if(pressed&(1u<<8)){if(category==C_ARP)nextSubPage();else setCategory(C_ARP);}\n    if(pressed&(1u<<9)){if(category==C_SEQ)nextSubPage();else setCategory(C_SEQ);}\n    if(pressed&(1u<<10))toggleSeqPlay();\n    if(pressed&(1u<<11))toggleSeqRecord();\n    if(pressed&(1u<<12))voice.note-=12,voice.targetHz=midiHz(voice.note);''',
"arp seq physical buttons")

# Audio-rate clock for the arp and sequencer. The 32 rhythm choices are original,
# deterministic patterns of increasing density rather than copied Novation patterns.
insert_before_range='''double clockStepSamples(int gridStep){\n    const double base=double(kRate)*60.0/double(std::max(40,arpTempo))/4.0;\n    const double amount=(double(arpSwing)-50.0)/50.0*0.45;\n    return base*((gridStep&1)?(1.0+amount):(1.0-amount));\n}\nint buildArpNotes(std::array<int,512>& out){\n    int base[128]{},count=0;\n    if(!arpLatch){for(int n=0;n<128;++n)arpHeld[size_t(n)]=rawHeld[size_t(n)];}\n    else{bool any=false;for(auto v:arpHeld)if(v){any=true;break;}if(!any)for(int n=0;n<128;++n)if(rawHeld[size_t(n)])arpHeld[size_t(n)]=1;}\n    if(arpMode==4){for(int i=0;i<playedOrderCount;++i){int n=playedOrder[size_t(i)];if(arpHeld[size_t(n)])base[count++]=n;}}\n    else{for(int n=0;n<128;++n)if(arpHeld[size_t(n)])base[count++]=n;}\n    int total=0;for(int o=0;o<arpOctaves;++o)for(int i=0;i<count&&total<int(out.size());++i)out[size_t(total++)]=std::clamp(base[i]+12*o,0,127);return total;\n}\nint nextArpNote(){\n    std::array<int,512> notes{};const int count=buildArpNotes(notes);if(count<=0)return -1;\n    if(arpMode==5)return notes[size_t((rngState>>8)%uint32_t(count))];\n    int idx=0;\n    if(arpMode==1)idx=count-1-(arpNoteIndex%count);\n    else if(arpMode==2){const int span=std::max(1,count*2-2),q=arpNoteIndex%span;idx=q<count?q:span-q;}\n    else if(arpMode==3){const int span=std::max(1,count*2),q=arpNoteIndex%span;idx=q<count?q:std::max(0,count-1-(q-count));}\n    else idx=arpNoteIndex%count;\n    ++arpNoteIndex;return notes[size_t(std::clamp(idx,0,count-1))];\n}\nvoid processArpSeqClock(){\n    if(arpGateCountdown>0&&--arpGateCountdown==0&&arpCurrentNote>=0){voiceNoteOff(arpCurrentNote);arpCurrentNote=-1;}\n    if(seqGateCountdown>0&&--seqGateCountdown==0&&seqCurrentNote>=0){voiceNoteOff(seqCurrentNote);seqCurrentNote=-1;}\n    if(seqPlaying){\n        if(seqSamplesUntil<=0.0){const int grid=seqGridStep++&31;const double interval=clockStepSamples(grid);if(rhythmHit(arpRhythm,grid)){const int len=seqLengthStored(seqSlot),st=seqPlayStep%len,type=seqStepTypeStored(seqSlot,st);if(type==2||seqPitchCode(seqSlot,st)==0){if(seqCurrentNote>=0)voiceNoteOff(seqCurrentNote);seqCurrentNote=-1;}else{const int note=std::clamp(seqTransposeRoot+seqOffsetStored(seqSlot,st),0,127);if(type==1&&seqCurrentNote>=0)voiceLegatoTo(note);else{if(seqCurrentNote>=0)voiceNoteOff(seqCurrentNote);voiceNoteOn(note,110);}seqCurrentNote=note;const int next=(st+1)%len;if(seqStepTypeStored(seqSlot,next)!=1)seqGateCountdown=int(interval*0.72);}++seqPlayStep;}if(seqRetrig&&grid==31)seqPlayStep=0;seqSamplesUntil+=interval;}seqSamplesUntil-=1.0;return;\n    }\n    if(arpEnabled){\n        if(arpSamplesUntil<=0.0){const int grid=arpGridStep++&31;const double interval=clockStepSamples(grid);if(rhythmHit(arpRhythm,grid)){const int note=nextArpNote();if(note>=0){if(arpCurrentNote>=0)voiceNoteOff(arpCurrentNote);voiceNoteOn(note,105);arpCurrentNote=note;arpGateCountdown=int(interval*0.68);}}arpSamplesUntil+=interval;}arpSamplesUntil-=1.0;return;\n    }\n    if(arpCurrentNote>=0){voiceNoteOff(arpCurrentNote);arpCurrentNote=-1;}\n}\n\n'''
marker='''float rangeMul(uint8_t v){'''
if insert_before_range not in s:
    if marker not in s: raise SystemExit("could not find rangeMul")
    s=s.replace(marker,insert_before_range+marker,1)

# Clock must run before envelopes are advanced for this sample.
rep(
'''float renderOne(){\n    stepEnv(voice.envAmp,voice.ampStage,current.p,P_AMP_A);''',
'''float renderOne(){\n    processArpSeqClock();\n    stepEnv(voice.envAmp,voice.ampStage,current.p,P_AMP_A);''',
"arp seq audio clock")

# Feed physical and MIDI notes through the performance router.
s=s.replace('if(m&(1u<<i))noteOn(n,105);else noteOff(n);','if(m&(1u<<i))inputNoteOn(n,105);else inputNoteOff(n);')
s=s.replace('if((s&0xF0)==0x90&&d2>0)noteOn(int(d1),int(d2));else if((s&0xF0)==0x80||((s&0xF0)==0x90&&d2==0))noteOff(int(d1));',
            'if((s&0xF0)==0x90&&d2>0)inputNoteOn(int(d1),int(d2));else if((s&0xF0)==0x80||((s&0xF0)==0x90&&d2==0))inputNoteOff(int(d1));')

# Reset performance clocks/pools on boot while preserving sequence flash memory.
rep(
'''voice=Voice{};category=C_HOME;subPage=0;syncPage();drawScreen();''',
'''voice=Voice{};rawHeld.fill(0);arpHeld.fill(0);playedOrderCount=0;arpEnabled=false;seqPlaying=false;seqRecording=false;seqSlot=0;seqEditStep=0;resetClockPositions();category=C_HOME;subPage=0;syncPage();drawScreen();''',
"boot performance reset")

if 'noteOn(int(d1)' in s or 'noteOff(int(d1)' in s:
    raise SystemExit('stale MIDI note routing remains')
if s == orig:
    raise SystemExit("no changes made")
path.write_text(s, encoding="utf-8")
print(f"patched BSII arp/sequencer in {path}")
