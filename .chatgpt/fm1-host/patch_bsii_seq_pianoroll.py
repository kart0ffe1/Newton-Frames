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

# Add a compact four-control header and a 32-step pitch piano roll for SEQ 2.
insert = r'''void drawSeqCompactControls(uint16_t dim,uint16_t pale,uint16_t deep,uint16_t blue){
    for(int k=0;k<4;++k){
        const int p=knobParam(k),x=7+k*58;char value[16]{};
        if(p==kNoParam)continue;
        text(x,66,paramName(p),dim,1);
        formatParamValue(p,value,sizeof(value));
        text(x,78,value,pale,1);
        fillRect(x,89,50,3,deep);
        fillRect(x,89,int(paramRawValue(p)*50/127),3,blue);
    }
}

bool pianoBlackKey(int pc){pc=((pc%12)+12)%12;return pc==1||pc==3||pc==6||pc==8||pc==10;}
void drawSequencePianoRoll(uint16_t note,uint16_t selected,uint16_t grid,uint16_t deep,uint16_t pale,uint16_t rest){
    const int len=seqLengthStored(seqSlot);
    const int rollX=34,stepW=6,rollTop=99,rowH=8,rows=16,rollBottom=rollTop+rows*rowH;

    int minPitch=0,maxPitch=0;bool havePitch=false;
    for(int st=0;st<len;++st){
        if(seqPitchCode(seqSlot,st)==0||seqStepTypeStored(seqSlot,st)==2)continue;
        const int p=seqOffsetStored(seqSlot,st);
        if(!havePitch){minPitch=maxPitch=p;havePitch=true;}else{minPitch=std::min(minPitch,p);maxPitch=std::max(maxPitch,p);}
    }
    int focus=0;
    if(seqPitchCode(seqSlot,seqEditStep)!=0&&seqStepTypeStored(seqSlot,seqEditStep)!=2)focus=seqOffsetStored(seqSlot,seqEditStep);
    else if(havePitch)focus=(minPitch+maxPitch)/2;
    int low=focus-7;
    if(havePitch&&maxPitch-minPitch<rows){low=(minPitch+maxPitch-(rows-1))/2;}
    low=std::clamp(low,-48,47-(rows-1));
    const int high=low+rows-1;

    // Tiny piano keyboard and horizontal semitone lanes.
    for(int r=0;r<rows;++r){
        const int pitch=high-r,y=rollTop+r*rowH;
        const bool black=pianoBlackKey(pitch);
        if(black)fillRect(7,y+1,20,rowH-1,deep);else fillRect(7,y+1,20,rowH-1,grid);
        if(black)fillRect(18,y+2,9,rowH-3,grid);
        line(rollX,y,rollX+32*stepW,y,black?deep:grid);
    }
    line(rollX,rollBottom,rollX+32*stepW,rollBottom,grid);

    // Step columns, stronger every four steps. Selected/edit and play positions are highlighted.
    const int playPos=seqPlaying&&len>0?(seqPlayStep%len):-1;
    for(int st=0;st<32;++st){
        const int x=rollX+st*stepW;
        if(st==seqEditStep)fillRect(x,rollTop,stepW-1,rows*rowH,deep);
        line(x,rollTop,x,rollBottom,(st%4)==0?grid:deep);
        if(st>=len)fillRect(x,rollBottom-3,stepW-1,3,deep);
        if(st==playPos)line(x+1,rollTop,x+stepW-2,rollTop,selected);
    }
    line(rollX+32*stepW,rollTop,rollX+32*stepW,rollBottom,grid);

    // Notes and legato connections.
    int prevX=-1,prevY=-1;bool prevActive=false;
    for(int st=0;st<len;++st){
        const int x=rollX+st*stepW;
        const int type=seqStepTypeStored(seqSlot,st);
        const bool active=seqPitchCode(seqSlot,st)!=0&&type!=2;
        if(!active){
            // A short dash at the bottom keeps rests visible without looking like notes.
            line(x+1,rollBottom-5,x+stepW-2,rollBottom-5,rest);
            prevActive=false;
            continue;
        }
        const int pitch=seqOffsetStored(seqSlot,st);
        if(pitch<low||pitch>high){
            // Small edge marker indicates a note outside the current vertical view.
            const int y=pitch>high?rollTop+1:rollBottom-3;
            fillRect(x+1,y,stepW-2,2,rest);
            prevActive=false;
            continue;
        }
        const int y=rollTop+(high-pitch)*rowH+2;
        const uint16_t c=st==seqEditStep?selected:note;
        if(type==1&&prevActive)line(prevX+2,prevY+2,x+1,y+2,note);
        fillRect(x+1,y,stepW-2,5,c);
        if(type==1)line(x+1,y+6,x+stepW-2,y+6,pale);
        prevX=x;prevY=y;prevActive=true;
    }

    // Range labels keep relative pitch readable without crowding the roll.
    char hi[8]{},lo[8]{};std::snprintf(hi,sizeof(hi),"%+d",high);std::snprintf(lo,sizeof(lo),"%+d",low);
    text(8,rollTop,hi,pale,1);text(8,rollBottom-7,lo,pale,1);
}

'''
marker = '''void drawScreen(){\n'''
if insert not in s:
    if marker not in s:
        raise SystemExit("could not find drawScreen insertion point")
    s = s.replace(marker, insert + marker, 1)

old_loop = r'''    for(int k=0;k<4;++k){
        int p=knobParam(k),x=(k%2)*116+10,y=(k/2)*55+68;char value[16]{};
        if(p==kNoParam)continue;
        text(x,y,paramName(p),dim,1);formatParamValue(p,value,sizeof(value));text(x,y+15,value,pale,2);
        int w=int(paramRawValue(p)*92/127);fillRect(x,y+36,94,7,deep);fillRect(x,y+36,w,7,blue);
    }'''
new_loop = r'''    if(page==13)drawSeqCompactControls(dim,pale,deep,blue);
    else for(int k=0;k<4;++k){
        int p=knobParam(k),x=(k%2)*116+10,y=(k/2)*55+68;char value[16]{};
        if(p==kNoParam)continue;
        text(x,y,paramName(p),dim,1);formatParamValue(p,value,sizeof(value));text(x,y+15,value,pale,2);
        int w=int(paramRawValue(p)*92/127);fillRect(x,y+36,94,7,deep);fillRect(x,y+36,w,7,blue);
    }'''
rep(old_loop, new_loop, "compact SEQ 2 controls")

rep(
'''    if(page==12||page==13)drawSequenceStrip(cyan,deep,dim);''',
'''    if(page==12)drawSequenceStrip(cyan,deep,dim);\n    if(page==13)drawSequencePianoRoll(cyan,pale,dim,deep,pale,blue);''',
"SEQ 2 piano roll")

if s == orig:
    raise SystemExit("no changes made")
path.write_text(s, encoding="utf-8")
print(f"patched SEQ 2 piano roll in {path}")
