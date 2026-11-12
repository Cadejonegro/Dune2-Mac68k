#include "DuneEffects.h"
#include <Sound.h>
#include <OSUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FX_RATE 22050UL
#define FX_COUNT 8
#define CUE_COUNT 15
typedef struct FxSample { unsigned long offset,length; const char *name; } FxSample;
typedef struct FxCue { short step; unsigned long delay; short sample; short voicePriority; } FxCue;
static const FxSample samples[FX_COUNT]={
 {0UL,58290UL,"BRAKES2P"},
 {58290UL,111680UL,"WIND2BP"},
 {169970UL,10729UL,"CLANK"},
 {180699UL,18226UL,"MISSLE8"},
 {198925UL,16360UL,"BLASTER"},
 {215285UL,2823UL,"GUNSHOT"},
 {218108UL,9411UL,"GLASS"},
 {227519UL,28790UL,"GLASS6"},

};
static const FxCue cues[CUE_COUNT]={
 {5,0UL,1,0}, /* wind */
 {7,900UL,0,1}, /* brakes voice-priority */
 {12,490UL,2,0}, /* tank +140 ticks */
 {14,45UL,5,0}, /* Atreides gun 1 */
 {14,75UL,6,0}, /* Atreides glass impact 1 */
 {14,140UL,5,0}, /* Atreides gun 2 */
 {14,170UL,7,0}, /* Atreides glass impact 2 */
 {15,30UL,3,0}, /* Ordos 1 */
 {15,125UL,3,0}, /* Ordos 2 */
 {16,30UL,3,0}, /* Ordos 3 */
 {16,125UL,3,0}, /* Ordos 4 */
 {18,45UL,4,0}, /* Harkonnen 1 */
 {18,120UL,4,0}, /* Harkonnen 2 */
 {19,45UL,4,0}, /* Harkonnen 3 */
 {19,125UL,4,0}, /* Harkonnen 4 */

};
static SndChannelPtr channel=NULL;
static SoundHeader *active=NULL;
static unsigned long activeEnd=0;
static unsigned char fired[CUE_COUNT]={0};
static short lastStep=-2;

static OSErr playEffect(short id){
 FILE *f;SoundHeader *h;SndCommand cmd;OSErr err;
 unsigned long bytes;
 if(id<0||id>=FX_COUNT)return paramErr;
 bytes=(unsigned long)sizeof(SoundHeader)+samples[id].length;
 h=(SoundHeader*)malloc((size_t)bytes);
 if(!h)return memFullErr;
 memset(h,0,(size_t)bytes);
 f=fopen("EFFECTS8.DAT","rb");
 if(!f){free(h);return fnfErr;}
 if(fseek(f,(long)samples[id].offset,SEEK_SET)!=0 ||
    fread(h->sampleArea,1,(size_t)samples[id].length,f)!=(size_t)samples[id].length){
  fclose(f);free(h);return eofErr;
 }
 fclose(f);
 if(!channel){err=SndNewChannel(&channel,sampledSynth,0,NULL);if(err!=noErr){free(h);return err;}}
 cmd.cmd=quietCmd;cmd.param1=0;cmd.param2=0;
 err=SndDoImmediate(channel,&cmd);
 if(err!=noErr){free(h);return err;}
 if(active)free(active);
 active=h;
 h->samplePtr=NULL;h->length=samples[id].length;
 h->sampleRate=FX_RATE<<16;h->encode=stdSH;h->baseFrequency=60;
 cmd.cmd=bufferCmd;cmd.param1=0;cmd.param2=(long)h;
 err=SndDoCommand(channel,&cmd,false);
 if(err!=noErr){free(active);active=NULL;return err;}
 activeEnd=TickCount()+(samples[id].length*60UL+FX_RATE-1)/FX_RATE;
 return noErr;
}
void DuneEffects_Update(short step,unsigned long elapsed,int voicePending,FILE *logFile){
 int i;OSErr err;unsigned long now=TickCount();
 if(step<0)return;
 if(step!=lastStep){lastStep=step;}
 for(i=0;i<CUE_COUNT;i++){
  if(fired[i]||cues[i].step!=step||elapsed<cues[i].delay)continue;
  if(cues[i].voicePriority && voicePending)continue;
  if(active && (long)(now-activeEnd)<0){
   /* Drop stale overlapping weapon events rather than firing them long after animation. */
   if(elapsed>cues[i].delay+35UL && !cues[i].voicePriority){fired[i]=1;}
   continue;
  }
  err=playEffect(cues[i].sample);
  if(logFile){fprintf(logFile,"effect step=%d delay=%lu actual=%lu name=%s result=%d tick=%lu voice=%d\n",step,cues[i].delay,elapsed,samples[cues[i].sample].name,(int)err,now,voicePending);fflush(logFile);}
  if(err==noErr)fired[i]=1;
 }
}
void DuneEffects_Shutdown(void){
 if(channel){SndDisposeChannel(channel,true);channel=NULL;}
 if(active){free(active);active=NULL;}
}
