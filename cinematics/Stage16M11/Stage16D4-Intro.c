/* Stage16C.5: subtitle preview based on original OpenDUNE intro sequence visual timing test.
   Predecoded WSA/CPS assets; 8-bit QuickDraw. Does not alter gameplay. */
#include <Quickdraw.h>
#include <Windows.h>
#include <Events.h>
#include <OSUtils.h>
#include <Fonts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DuneAudio.h"
#include "DuneEffects.h"
#include "VoiceIndex.h"
#define SW 320
#define SH 200
#define MAXPIX (SW*SH)
#define CW 344
#define CH 224

typedef struct Scene { const char *data; const char *pal; short w,h,frames; short ticks; short x,y; } Scene;
static const Scene scenes[] = {
 {"WESTWOOD.FRM","WESTWOOD.PAL",320,112,18,1,0,44},
 {"AND.FRM","AND.PAL",320,200,1,1,0,0},
 {"VIRGIN.FRM","VIRGIN.PAL",320,200,1,1,0,0},
 {"INTRO1.FRM","INTRO.PAL",304,120,50,1,8,40},
 {"INTRO2.FRM","INTRO.PAL",304,120,50,1,8,40},
 {"INTRO3.FRM","INTRO.PAL",304,120,40,1,8,40},
 {"INTRO9.FRM","INTRO.PAL",304,120,71,1,8,40},
 {"INTRO10.FRM","INTRO.PAL",304,120,1,1,8,40},
 {"EFINALA.FRM","INTRO.PAL",304,120,49,1,8,40},
 {"INTRO11.FRM","INTRO.PAL",304,120,45,1,8,40},
 {"INTRO4.FRM","INTRO.PAL",304,120,53,1,8,40},
 {"INTRO6.FRM","INTRO.PAL",304,120,75,1,8,40},
 {"INTRO7A.FRM","INTRO.PAL",304,120,27,1,8,40},
 {"INTRO7B.FRM","INTRO.PAL",304,120,22,1,8,40},
 {"INTRO8A.FRM","INTRO.PAL",304,120,16,1,8,40},
 {"INTRO8B.FRM","INTRO.PAL",304,120,19,1,8,40},
 {"INTRO8C.FRM","INTRO.PAL",304,120,29,1,8,40},
 {"INTRO5.FRM","INTRO.PAL",304,120,4,1,8,40},
};
typedef struct Step { short scene; unsigned short duration, frameCount, flags; } Step;
static const Step steps[] = {
 {-1,20,10,0x40},
 {3,45,0,0x62},
 {-1,50,10,0x48},
 {4,80,0,0x46},
 {-1,20,10,0x8},
 {5,100,0,0x46},
 {-1,30,10,0x0},
 {6,200,0,0x8A},
 {7,75,15,0x45},
 {8,57,0,0x42},
 {-1,1,2,0x0},
 {9,63,0,0x4A},
 {10,130,0,0x4E},
 {-1,40,10,0x0},
 {11,90,0,0x46},
 {12,55,0,0x402},
 {13,45,0,0x2},
 {14,30,0,0x402},
 {15,30,0,0x2},
 {16,50,0,0x12},
 {17,90,40,0x4D},
 {-1,100,50,0x0},
};
#define STEP_COUNT 22
#define SCENE_COUNT 18
static GWorldPtr world=NULL;
static CTabHandle colors=NULL;
static PaletteHandle palette=NULL;
static unsigned char *animationFrameBuffer=NULL;
static FILE *dataFile=NULL;
static WindowPtr window=NULL;
static int currentScene=-1,currentFrame=0;
static int currentStep=-1,displayFrame=0,stepFrames=1;
static unsigned long stepEnd=0,frameDelay=1;
static int finished=0;
static unsigned long nextTick=0;
static int running=1,paused=0;
static int lastVoiceSubtitle=-1;
static OSErr audioInitResult=noErr;
static unsigned long pauseStarted=0;
static int cachedFrameValid=0;
static FILE *profileFile=NULL;
static const char *exitReason="main loop ended";
static unsigned long scheduleLate=0,scheduleMaxLate=0,scheduleEvents=0;
static unsigned long setupTicks=0,initialFrameTicks=0;
static void setExit(const char *reason) { exitReason=reason; running=0; }
static void logEvent(const char *event) {
 if(profileFile){fprintf(profileFile,"event=%s scene=%d step=%d frame=%d tick=%lu\n",event,currentScene,currentStep,currentFrame,TickCount());fflush(profileFile);}
}
static unsigned long profileRead=0,profilePrep=0,profileCaption=0,profileBlit=0,profileFrames=0,profileSkipped=0;
static void profileReport(void) {
 if(profileFile && profileFrames) {
  fprintf(profileFile,"scene=%d step=%d frames=%lu skipped=%lu readTicks=%lu prepTicks=%lu captionTicks=%lu blitTicks=%lu\n",currentScene,currentStep,profileFrames,profileSkipped,profileRead,profilePrep,profileCaption,profileBlit);
  fprintf(profileFile,"schedule scene=%d step=%d lateEvents=%lu lateTicks=%lu maxLateTicks=%lu setupTicks=%lu firstFrameTicks=%lu\n",currentScene,currentStep,scheduleEvents,scheduleLate,scheduleMaxLate,setupTicks,initialFrameTicks);
  fflush(profileFile);
 }
 profileRead=profilePrep=profileCaption=profileBlit=profileFrames=profileSkipped=0;
 scheduleLate=scheduleMaxLate=scheduleEvents=setupTicks=initialFrameTicks=0;
}
static void cleanup(void) {
 if(dataFile){fclose(dataFile);dataFile=NULL;}
 if(world){DisposeGWorld(world);world=NULL;}
 cachedFrameValid=0;
 if(palette){DisposePalette(palette);palette=NULL;}
 if(colors){DisposeHandle((Handle)colors);colors=NULL;}
 if(animationFrameBuffer){free(animationFrameBuffer);animationFrameBuffer=NULL;}
 profileReport();
 if(profileFile){fclose(profileFile);profileFile=NULL;}
}
static int setupScene(int index) {
 unsigned char rgb[768]; FILE *pf; ColorTable *ct; Rect r; int i;
 profileReport();
 const Scene *s=&scenes[index];
 if(dataFile){fclose(dataFile);dataFile=NULL;}
 if(world){DisposeGWorld(world);world=NULL;}
 cachedFrameValid=0;
 if(palette){DisposePalette(palette);palette=NULL;}
 if(colors){DisposeHandle((Handle)colors);colors=NULL;}
 pf=fopen(s->pal,"rb"); if(!pf)return 0;
 if(fread(rgb,1,768,pf)!=768){fclose(pf);return 0;}
 fclose(pf);
 dataFile=fopen(s->data,"rb");if(!dataFile)return 0;
 colors=(CTabHandle)NewHandle(sizeof(ColorTable)+255*sizeof(ColorSpec));if(!colors)return 0;
 HLock((Handle)colors);ct=*colors;ct->ctSeed=0;ct->ctFlags=0;ct->ctSize=255;
 for(i=0;i<256;i++){
  ct->ctTable[i].value=i;
  ct->ctTable[i].rgb.red=(unsigned short)((unsigned long)rgb[i*3]*65535UL/63);
  ct->ctTable[i].rgb.green=(unsigned short)((unsigned long)rgb[i*3+1]*65535UL/63);
  ct->ctTable[i].rgb.blue=(unsigned short)((unsigned long)rgb[i*3+2]*65535UL/63);
 }
 HUnlock((Handle)colors);
 palette=NewPalette(256,colors,0,0);if(!palette)return 0;
 NSetPalette(window,palette,0);ActivatePalette(window);
 SetRect(&r,0,0,CW,CH);
 if(NewGWorld(&world,8,&r,colors,NULL,0)!=noErr)return 0;
 currentScene=index;currentFrame=0;
 nextTick=TickCount()+s->ticks;
 return 1;
}

/* Stage 16C.6: original English text and OpenDUNE subtitle event associations.
   Original fade parameters are retained; QuickDraw renders a discrete approximation
   of palette fades, not the original DOS palette-index animation. */
typedef struct Subtitle {
 short step, top, waitIn, fadeIn, waitOut, fadeOut;
 const char *text;
} Subtitle;
static Subtitle subtitles[] = {
 {0,94,1,2,7,2, ""},
 {2,104,1,5,7,64, ""},
 {3,154,3,5,35,5, ""},
 {5,154,1,4,16,4, ""},
 {5,154,3,4,16,4, ""},
 {7,154,1,5,25,5, ""},
 {7,154,2,5,27,9, ""},
 {8,154,1,5,10,5, ""},
 {9,154,1,4,24,4, ""},
 {11,154,1,4,20,3, ""},
 {11,154,1,3,20,4, ""},
 {12,154,10,3,30,4, ""},
 {13,85,0,2,10,1, ""},
 {14,154,32,2,35,7, ""},
 {15,154,5,2,26,7, ""},
 {17,154,1,2,34,7, ""},
 {20,154,1,4,29,5, ""},
 {21,85,0,3,20,0, ""},
 {21,85,0,0,3,15, ""}
};
#define SUBTITLE_COUNT (sizeof(subtitles)/sizeof(subtitles[0]))

#define LOCAL_SUBTITLE_MAX 256
static char localSubtitleText[SUBTITLE_COUNT][LOCAL_SUBTITLE_MAX];

static void loadLocalSubtitles(void) {
 FILE *f;
 char line[LOCAL_SUBTITLE_MAX * 2];
 int i;
 f=fopen("INTROTXT.DAT","r");
 if(!f)return;
 for(i=0;i<(int)SUBTITLE_COUNT;i++){
  size_t n,j,k;
  if(!fgets(line,sizeof(line),f))break;
  n=strcspn(line,"\r\n");
  line[n]=0;
  k=0;
  for(j=0;j<n && k<LOCAL_SUBTITLE_MAX-1;j++){
   if(line[j]=='\\' && j+1<n && line[j+1]=='n'){
    localSubtitleText[i][k++]='\n';
    j++;
   }else{
    localSubtitleText[i][k++]=line[j];
   }
  }
  localSubtitleText[i][k]=0;
  subtitles[i].text=localSubtitleText[i];
 }
 fclose(f);
}

static unsigned long stepStart=0;


static int lastSubtitle=-2,lastShade=-1;
static unsigned long lastCaptionRefresh=0;
/* Stage16D6.5: timeline-based narration. Event timings are a Mac adaptation,
   not yet verified against the original DOS voice script. */
typedef struct VoiceEvent {
 short step;
 unsigned short delayTicks;
 short clips[9];
} VoiceEvent;
/* Every event is independent of subtitle transitions. The final NOW is explicit. */
static const VoiceEvent voiceEvents[] = {
 {2,0,{2,-1}},                         /* Dune title announcement, held intro frame */
 {3,18,{0,1,-1}},                    /* KNOWN includes Dune; no DUNE title clip */
 {5,6,{3,-1}},                         /* Land of sand */
 {5,270,{4,5,6,-1}},                   /* Home of the Spice Melange */
 {7,6,{11,7,8,-1}},                     /* Spice controls Empire */
 {7,600,{9,10,38,-1}},              /* Whoever + controls Dune + controls the spice */
 {8,6,{42,12,13,-1}},                     /* The Emperor has proposed a challenge to each of the houses */
 {9,6,{14,15,16,36,-1}},           /* Emperor: the house that produces the most spice will control Dune */
 {11,6,{18,17,-1}},                    /* Emperor: There are no set territories */
 {11,190,{19,20,-1}},                  /* Emperor: And no rules of engagement */
 {12,6,{21,22,-1}},                    /* Vast armies arrived */
 {13,6,{35,23,39,32,-1}},               /* Tentative: ANDNOW + 3HOUSES + CONTROL4 + OFDUNE; verify ANDNOW */
 {14,6,{24,-1}},                       /* Atreides */
 {15,6,{25,26,-1}},                    /* Ordos */
 {17,6,{27,28,-1}},                    /* Harkonnen */
 {20,6,{37,29,-1}},                       /* Only one house will prevail */
 {21,6,{30,31,33,-1}},              /* Your battle for Dune begins: BATTLE includes for Dune */
 {21,310,{34,-1}}                      /* NOW: distinct final event */
};
#define VOICE_EVENT_COUNT (sizeof(voiceEvents)/sizeof(voiceEvents[0]))
static unsigned char eventQueued[VOICE_EVENT_COUNT];
static short voiceQueue[96];
static int queueHead=0,queueTail=0;
static void updateVoice(void){
 unsigned long elapsed;
 unsigned int i,j;
 short clip;
 OSErr err;
 if(paused||currentStep<0||audioInitResult!=noErr)return;
 elapsed=TickCount()-stepStart;
 if(!finished){
  for(i=0;i<VOICE_EVENT_COUNT;i++){
   const VoiceEvent *v=&voiceEvents[i];
   if(!eventQueued[i] && v->step==currentStep && elapsed>=v->delayTicks){
    eventQueued[i]=1;
    if(profileFile){fprintf(profileFile,"event voice=%u step=%d tick=%lu\n",i,currentStep,TickCount());fflush(profileFile);}
    for(j=0;j<9 && v->clips[j]>=0;j++){
     if(queueTail<(int)(sizeof(voiceQueue)/sizeof(voiceQueue[0])))voiceQueue[queueTail++]=v->clips[j];
    }
   }
  }
 }
 if(queueHead>=queueTail||DuneAudio_Busy())return;
 clip=voiceQueue[queueHead];
 err=DuneAudio_Play(clip);
 if(profileFile){fprintf(profileFile,"sample eventQueue=%d step=%d clip=%d name=%s tick=%lu result=%d\n",queueHead,currentStep,clip,voiceIndex[clip].name,TickCount(),(int)err);fflush(profileFile);}
 if(err==noErr)queueHead++;
 else if(profileFile){fprintf(profileFile,"audio playback failed; queue retained\n");fflush(profileFile);}
}


static void drawSubtitle(void) {
 unsigned long elapsed, total, begin, end, fadeTicks, outTicks, age;
 int i, first=-1, count=0, chosen=-1, shade=0;
 const Subtitle *sub;
 const char *line,*p;
 short lineCount=1, maxWidth=0, width, x, y, lineNo;
 Rect area;
 if(currentStep<0 || currentStep>=STEP_COUNT){lastSubtitle=-1;return;}
 total=(unsigned long)steps[currentStep].duration*6UL;
 elapsed=TickCount()-stepStart;
 for(i=0;i<(int)SUBTITLE_COUNT;i++)if(subtitles[i].step==currentStep){if(first<0)first=i;count++;}
 if(first<0 || elapsed>=total){lastSubtitle=-1;return;}
 /* Multiple captions on a scene receive separate, sequential windows. */
 for(i=0;i<count;i++){
  begin=total*(unsigned long)i/(unsigned long)count;
  end=total*(unsigned long)(i+1)/(unsigned long)count;
  if(elapsed>=begin && elapsed<end){chosen=first+i;break;}
 }
 if(chosen<0){lastSubtitle=-1;return;}
 sub=&subtitles[chosen];
  if(!sub->text || !sub->text[0]){lastSubtitle=-1;return;}
 begin=total*(unsigned long)(chosen-first)/(unsigned long)count;
 end=total*(unsigned long)(chosen-first+1)/(unsigned long)count;
 age=elapsed-begin;
 /* Retain original timing weights while keeping each caption within its scene. */
 if(age<(unsigned long)sub->waitIn*6UL && (unsigned long)sub->waitIn*6UL<(end-begin)/2UL){lastSubtitle=-1;return;}
 fadeTicks=(unsigned long)sub->fadeIn*6UL;
 outTicks=(unsigned long)sub->fadeOut*6UL;
 if(fadeTicks>(end-begin)/3UL)fadeTicks=(end-begin)/3UL;
 if(outTicks>(end-begin)/3UL)outTicks=(end-begin)/3UL;
 shade=2;
 if(fadeTicks && age<fadeTicks)shade=(age*3UL>=fadeTicks*2UL)?2:1;
 if(outTicks && elapsed>end-outTicks)shade=((end-elapsed)*3UL>=outTicks*2UL)?2:1;
 SetPort((GrafPtr)world);TextFont(0);TextSize(10);
 line=sub->text;
 for(p=line;*p;p++)if(*p=='\n')lineCount++;
 for(p=line;*p;){
  const char *e=strchr(p,'\n');
  short n=(short)(e?(e-p):strlen(p));
  width=TextWidth((char*)p,0,n);
  if(width>maxWidth)maxWidth=width;
  p=e?e+1:p+n;
 }
 if(maxWidth>296)maxWidth=296;
 /* Original top coordinates refer to the 320x200 cinematic surface. */
 /* Stage 16C.6.2: bottom-align subtitles within the
    320x200 cinematic viewport, including multiline captions. */
 y=(short)(205-lineCount*12);
 SetPort((GrafPtr)world);
 TextFont(0);TextSize(10);
 SetRect(&area,(short)(172-maxWidth/2-5),(short)(y-3),
              (short)(172+maxWidth/2+5),(short)(y+lineCount*12+2));
 ForeColor(blackColor);BackColor(whiteColor);PaintRect(&area);
 ForeColor(whiteColor);
 p=line;
 for(lineNo=0;lineNo<lineCount;lineNo++){
  const char *e=strchr(p,'\n');
  short n=(short)(e?(e-p):strlen(p));
  width=TextWidth((char*)p,0,n);
  x=(short)(172-width/2);
  MoveTo(x,(short)(y+9+lineNo*12));
  DrawText((char*)p,0,n);
  p=e?e+1:p+n;
 }
 ForeColor(blackColor);BackColor(whiteColor);
 lastSubtitle=chosen;lastShade=shade;
}

/* Render the cached decoded frame and caption entirely offscreen. */
static int presentCached(void) {
 PixMapHandle pm; Rect src,dst; unsigned char *base; int stride,y; unsigned long t;
 const Scene *scene;
 if(!world || !window || !cachedFrameValid || currentScene<0)return 0;
 scene=&scenes[currentScene];
 t=TickCount();
 pm=GetGWorldPixMap(world);
 if(!pm || !LockPixels(pm))return 0;
 stride=(**pm).rowBytes&0x3fff;
 base=(unsigned char*)GetPixBaseAddr(pm);
 if(!base || stride<CW){UnlockPixels(pm);return 0;}
 for(y=0;y<CH;y++)memset(base+y*stride,0,CW);
 for(y=0;y<scene->h;y++)
  memcpy(base+(y+scene->y+20)*stride+scene->x+12,
         animationFrameBuffer+y*scene->w,scene->w);
 UnlockPixels(pm);
 profilePrep+=TickCount()-t;
 t=TickCount();
 /* All subtitle drawing happens before the screen is updated. */
 drawSubtitle();
 profileCaption+=TickCount()-t;
 t=TickCount();
 pm=GetGWorldPixMap(world);
 if(!LockPixels(pm))return 0;
 SetPort(window);
 SetRect(&src,0,0,CW,CH);SetRect(&dst,0,0,CW,CH);
 ForeColor(blackColor);BackColor(whiteColor);
 CopyBits((BitMap*)*pm,(BitMap*)*((CGrafPtr)window)->portPixMap,&src,&dst,srcCopy,NULL);
 UnlockPixels(pm);
 profileBlit+=TickCount()-t;
 profileFrames++;
 if(profileFrames>=120)profileReport();
 return 1;
}
static int drawCurrent(void) {
 const Scene *scene=&scenes[currentScene];
 long offset=(long)currentFrame*scene->w*scene->h; unsigned long t=TickCount();
 if(fseek(dataFile,offset,SEEK_SET)!=0)return 0;
 if(fread(animationFrameBuffer,1,(size_t)scene->w*scene->h,dataFile)!=(size_t)scene->w*scene->h)return 0;
 profileRead+=TickCount()-t;
 cachedFrameValid=1;
 return presentCached();
}
/* Caption transitions are the only reason to re-present a held frame. */
static int activeCaption(void) {
 unsigned long total,elapsed,begin,end,age;
 int i,first=-1,count=0,slot;
 const Subtitle *sub;
 if(currentStep<0 || currentStep>=STEP_COUNT)return -1;
 total=(unsigned long)steps[currentStep].duration*6UL;
 elapsed=TickCount()-stepStart;
 if(elapsed>=total)return -1;
 for(i=0;i<(int)SUBTITLE_COUNT;i++)if(subtitles[i].step==currentStep){if(first<0)first=i;count++;}
 if(count==0)return -1;
 slot=(int)(elapsed*(unsigned long)count/total);
 if(slot>=count)return -1;
 begin=total*(unsigned long)slot/(unsigned long)count;
 end=total*(unsigned long)(slot+1)/(unsigned long)count;
 sub=&subtitles[first+slot];
  if(!sub->text || !sub->text[0])return -1;
 age=elapsed-begin;
 if(age<(unsigned long)sub->waitIn*6UL && (unsigned long)sub->waitIn*6UL<(end-begin)/2UL)return -1;
 return first+slot;
}

static int enterStep(int index) {
 const Step *st;
 unsigned long now, durationTicks;
 int mode, fcount;
 if(index>=STEP_COUNT) {finished=1;logEvent("sequence complete");return 1;}
 currentStep=index;
 st=&steps[index];
 now=TickCount();
 lastSubtitle=-2;lastShade=-1;
 durationTicks=(unsigned long)st->duration*6UL;
 mode=st->flags&3;
 if(st->scene>=0){
  if(!setupScene(st->scene)){logEvent("setupScene failed");return 0;}
  setupTicks=TickCount()-now;
  displayFrame=0;
  if(mode==3)displayFrame=st->frameCount;
  fcount=scenes[st->scene].frames;
  if(mode==1 && st->frameCount>0)stepFrames=st->frameCount;
  else if(mode==3)stepFrames=20;
  else stepFrames=fcount;
  if(stepFrames<1)stepFrames=1;
  frameDelay=durationTicks/(unsigned long)stepFrames;
  if(frameDelay<1)frameDelay=1;
  currentFrame=displayFrame;
  now=TickCount();
  if(!drawCurrent()){logEvent("first frame read/render failed");return 0;}
  initialFrameTicks=TickCount()-now;
  /* Scene setup is excluded from playback duration. First-frame drawing
     is included in the playback budget; deadline anchored after setup. */
  stepStart=now;stepEnd=now+durationTicks;
  nextTick=now+frameDelay;
 } else {
  stepFrames=1;
  stepStart=TickCount();stepEnd=stepStart+durationTicks;
  nextTick=stepEnd;
  if(currentScene>=0 && !presentCached()){logEvent("held frame render failed");return 0;}
 }
 return 1;
}
static int advanceFrame(void){
 const Step *st=&steps[currentStep];
 int mode=st->flags&3;
 if(st->scene<0 || mode==0 || mode==3)return 1;
 displayFrame++;
 if(displayFrame>=scenes[currentScene].frames){
  if(mode==1)displayFrame=0;
  else {displayFrame=scenes[currentScene].frames-1;return 1;}
 }
 currentFrame=displayFrame;
 return drawCurrent();
}
int main(void){Rect r;EventRecord e;WindowPtr hit;unsigned char key;
 InitGraf(&qd.thePort);InitFonts();InitWindows();InitMenus();TEInit();InitDialogs(NULL);InitCursor();
 animationFrameBuffer=(unsigned char*)malloc(MAXPIX);if(!animationFrameBuffer)return 1;
 SetRect(&r,40,40,384,264);
 window=NewCWindow(NULL,&r,"\pStage 16M.10 - Timed Effects",true,documentProc,(WindowPtr)-1,true,0);
 if(!window){cleanup();return 2;}
 profileFile=fopen("D68PROFILE.TXT","w");
 if(profileFile){fprintf(profileFile,"Stage16D6.8 event timeline: 60 ticks/second\n");fflush(profileFile);}
 loadLocalSubtitles();
  audioInitResult=DuneAudio_Init();
 if(!setupScene(0)){DuneAudio_Shutdown();cleanup();return 3;}
 SetPort(window);EraseRect(&window->portRect);
 if(!drawCurrent()){cleanup();return 4;}
 nextTick=TickCount()+18*6;
 while(running){
  if(WaitNextEvent(everyEvent,&e,1,NULL)){
   switch(e.what){
    case keyDown:case autoKey:
     key=(unsigned char)(e.message&charCodeMask);
     if(key==27||key=='q')setExit("user pressed Q/Escape");
     else if(key==' '){if(!paused){paused=1;pauseStarted=TickCount();}else{unsigned long delta=TickCount()-pauseStarted;paused=0;nextTick+=delta;stepEnd+=delta;stepStart+=delta;}}
     else if(key=='n'){
      if(currentStep<0){if(!setupScene(1)||!drawCurrent())setExit("logo scene 1 failed");nextTick=TickCount()+120;}
      else if(!enterStep(currentStep+1))setExit("enterStep failed");
     }
     break;
    case mouseDown:
     if(FindWindow(e.where,&hit)==inGoAway&&hit==window)setExit("window close");
     break;
    case updateEvt:
     BeginUpdate((WindowPtr)e.message);if(!presentCached())setExit("update render failed");EndUpdate((WindowPtr)e.message);
     break;
    default:break;
   }
  }
  if(running&&!paused){
   updateVoice();
   DuneEffects_Update(currentStep,TickCount()-stepStart,
     (DuneAudio_Busy() || queueHead<queueTail),profileFile);
   unsigned long now=TickCount();
   if(currentStep<0){
    if((long)(now-nextTick)>=0){
     if(currentScene==0){
      currentFrame++;
      if(currentFrame<18){if(!drawCurrent())setExit("frame read/render failed");nextTick=now+6;}
      else {if(!setupScene(1)||!drawCurrent())setExit("logo scene 1 failed");nextTick=now+180;}
     }else if(currentScene==1){if(!setupScene(2)||!drawCurrent())setExit("logo scene 2 failed");nextTick=now+300;}
     else {if(!enterStep(0))setExit("initial enterStep failed");}
    }
   }else if(!finished){
    if((long)(now-stepEnd)>=0){if(!enterStep(currentStep+1))setExit("enterStep failed");}
    else if((long)(now-nextTick)>=0){
     unsigned long due=1UL+(now-nextTick)/frameDelay;
     const Step *st=&steps[currentStep];
     int mode=st->flags&3;
     if(st->scene>=0 && (mode==1 || mode==2)){
      unsigned long count=(unsigned long)scenes[currentScene].frames;
      if(mode==1)displayFrame=(int)(((unsigned long)displayFrame+due)%count);
      else {
       unsigned long target=(unsigned long)displayFrame+due;
       displayFrame=(int)(target>=count ? count-1 : target);
      }
      currentFrame=displayFrame;
      if(!drawCurrent())setExit("frame read/render failed");
     }else if(!advanceFrame())setExit("advanceFrame failed");
     if(due>1)profileSkipped+=due-1;
     scheduleEvents++;
     scheduleLate+=now-nextTick;
     if(now-nextTick>scheduleMaxLate)scheduleMaxLate=now-nextTick;
     nextTick+=due*frameDelay;
    }
    else if(currentScene>=0 && (long)(now-stepStart)>=0 &&
            (now-lastCaptionRefresh)>=6UL && (long)(now-nextTick)<0){
     /* Only re-present on caption transitions; no disk I/O. */
     if(activeCaption()!=lastSubtitle){
      if(!presentCached())setExit("caption redraw failed");
     }
     lastCaptionRefresh=now;
    }
   }
  }
 }
 if(profileFile){fprintf(profileFile,"exit reason=%s scene=%d step=%d finished=%d tick=%lu\n",exitReason,currentScene,currentStep,finished,TickCount());fflush(profileFile);}
 DuneEffects_Shutdown();DuneAudio_Shutdown();cleanup();return 0;
}
