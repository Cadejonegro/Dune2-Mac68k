#include "DuneAudio.h"
#include "VoiceIndex.h"
#include <Sound.h>
#include <OSUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Single Sound Manager channel, one clip resident at a time. */
static SndChannelPtr channel=NULL;
static SoundHeader *active=NULL;
static FILE *pack=NULL;
static unsigned long clipEnd=0;

OSErr DuneAudio_Init(void){
 OSErr err;
 if(channel)return noErr;
 pack=fopen("INTROVOX.DAT","rb");
 if(!pack)return fnfErr;
 err=SndNewChannel(&channel,sampledSynth,0,NULL);
 if(err!=noErr){fclose(pack);pack=NULL;channel=NULL;}
 return err;
}
int DuneAudio_Busy(void){return active && (long)(TickCount()-clipEnd)<0;}
OSErr DuneAudio_Play(short id){
 const VoiceIndex *v;
 SndCommand cmd;
 SoundHeader *h;
 OSErr err;
 unsigned long bytes;
 if(id<0 || (unsigned long)id>=VOICE_COUNT)return paramErr;
 if(!channel){err=DuneAudio_Init();if(err!=noErr)return err;}
 v=&voiceIndex[id];
 bytes=(unsigned long)sizeof(SoundHeader)+v->length;
 h=(SoundHeader*)malloc((size_t)bytes);
 if(!h)return memFullErr;
 memset(h,0,(size_t)bytes);
 if(fseek(pack,(long)v->offset,SEEK_SET)!=0 || fread(h->sampleArea,1,(size_t)v->length,pack)!=(size_t)v->length){free(h);return eofErr;}
 h->samplePtr=NULL;h->length=v->length;h->sampleRate=v->rate<<16;
 h->encode=stdSH;h->baseFrequency=60;
 cmd.cmd=quietCmd;cmd.param1=0;cmd.param2=0;
 err=SndDoImmediate(channel,&cmd);
 if(err!=noErr){free(h);return err;}
 if(active){free(active);active=NULL;}
 active=h;
 cmd.cmd=bufferCmd;cmd.param1=0;cmd.param2=(long)h;
 err=SndDoCommand(channel,&cmd,false);
 if(err!=noErr){free(active);active=NULL;return err;}
 clipEnd=TickCount()+(v->length*60UL+v->rate-1)/v->rate;
 return noErr;
}
void DuneAudio_Shutdown(void){
 if(channel){SndDisposeChannel(channel,true);channel=NULL;}
 if(active){free(active);active=NULL;}
 if(pack){fclose(pack);pack=NULL;}
}
