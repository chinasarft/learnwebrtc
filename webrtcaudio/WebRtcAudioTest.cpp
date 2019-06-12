// WebRtcAudioTest.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <Windows.h>
#endif

#include "modules/audio_processing/aec/echo_cancellation.h"
#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/ns/noise_suppression.h"
//#include "common_audio/signal_processing/include/signal_processing_library.h"

using namespace webrtc;

int WebRtcAecTest()
{
#define  NN 160
        float far_frame[NN];
        float near_frame[NN];
        float out_frame[NN];
        
        void *aecmInst = NULL;
        FILE *fp_far  = fopen("speaker.pcm", "rb");
        FILE *fp_near = fopen("micin.pcm", "rb");
        FILE *fp_out  = fopen("aecout.pcm", "wb");
        
        do
        {
                if(!fp_far || !fp_near || !fp_out)
                {
                        printf("WebRtcAecTest open file err \n");
                        break;
                }
                
                aecmInst = WebRtcAec_Create();
                WebRtcAec_Init(aecmInst, 8000, 8000);
                
                AecConfig config;
                config.nlpMode = kAecNlpConservative;
                WebRtcAec_set_config(aecmInst, config);
                
                while(1)
                {
                        if (NN == fread(far_frame, sizeof(float), NN, fp_far))
                        {
                                fread(near_frame, sizeof(float), NN, fp_near);
                                WebRtcAec_BufferFarend(aecmInst, far_frame, NN);//对参考声音(回声)的处理
                                /*
                                 int32_t WebRtcAec_Process(
                                 void* aecInst,
                                 const float* const* nearend,
                                 size_t num_bands,
                                 float* const* out,
                                 size_t nrOfSamples,
                                 int16_t msInSndCardBuf,
                                 int32_t skew);*/
                                float *nearFrame[1] = {(float*)near_frame};
                                float *outFrame[1] = {(float*)out_frame};
                                WebRtcAec_Process(aecmInst, (const float* const*)nearFrame, 1, (float* const*)outFrame, NN, 40, 0);//回声消除
                                fwrite(out_frame, sizeof(float), NN, fp_out);
                        }
                        else
                        {
                                break;
                        }
                }
        } while (0);
        
        fclose(fp_far);
        fclose(fp_near);
        fclose(fp_out);
        WebRtcAec_Free(aecmInst);
        return 0;
}

void WebRtcNS32KSample(const char *szFileIn, const char *szFileOut,int nSample,int nMode)
{
	NsHandle *pNS_inst = NULL;

	FILE *fpIn = NULL;
	FILE *fpOut = NULL;

	char *pInBuffer =NULL;
	char *pOutBuffer = NULL;
        int ret = 0;

	do
	{
		int i = 0;
		int nFileSize = 0;
		pNS_inst = WebRtcNs_Create();
                if ( (ret = WebRtcNs_Init(pNS_inst,nSample)) != 0) {
                        fprintf(stderr, "WebRtcNs_Init:%d\n", ret);
                        return;
                }
                if ( (ret = WebRtcNs_set_policy(pNS_inst,nMode)) != 0) {
                        fprintf(stderr, "WebRtcNs_Init:%d\n", ret);
                        return;
                }

		fpIn = fopen(szFileIn, "rb");
		fpOut = fopen(szFileOut, "wb");
		if (NULL == fpIn || NULL == fpOut)
		{
			printf("open file err \n");
			break;
		}

		fseek(fpIn,0,SEEK_END);
		nFileSize = ftell(fpIn); 
		fseek(fpIn,0,SEEK_SET); 

		pInBuffer = (char*)malloc(nFileSize);
		fread(pInBuffer, sizeof(char), nFileSize, fpIn);

		pOutBuffer = (char*)malloc(nFileSize);
                memset(pOutBuffer, 0, nFileSize);
               
                int num_band = (nSample/100 + 159)/160;
                int step = nSample/100 * sizeof(float);
                
                 // band参数，大概是这样: 最多处理160个samples，所以对于采样率大于16000的，10ms会超过
                 // 实际上应该只有32000的采样率这个了，就会分成两个band
                //if (step > 160 * sizeof(float))
                //        step = 160 * sizeof(float);
                printf("step=%d band=%d\n", step, num_band);
		for (i = 0;i < nFileSize; i+=step)
		{
                        if (nFileSize - i < step) {
                                break;
                        }
                        float *pIn[3] = {(float*)(pInBuffer+i)};
                        float *pOut[3] = {(float*)(pOutBuffer+i)};
                        for (int i = 1; i < num_band; i++) {
                                pIn[i] = (float*)((pInBuffer+i) + (sizeof(float) * 160));
                                pOut[i] = (float*)((pOutBuffer+i) + (sizeof(float) * 160));
                        }
                        // TODO 不知道区别
                        //for(int i = 0; i < num_band; i++) WebRtcNs_Analyze(pNS_inst, pIn[i]);
                        WebRtcNs_Analyze(pNS_inst, pIn[0]);
                        WebRtcNs_Process(pNS_inst ,(const float* const*)(pIn), num_band, (float* const*)(pOut));
		}

		fwrite(pOutBuffer, sizeof(char), nFileSize, fpOut);
	} while (0);

	WebRtcNs_Free(pNS_inst);
	fclose(fpIn);
	fclose(fpOut);
	free(pInBuffer);
	free(pOutBuffer);
}
/*
void WebRtcAgcTest(char *filename, char *outfilename,int fs)
{
	FILE *infp      = NULL;
	FILE *outfp     = NULL;

	short *pData    = NULL;
	short *pOutData = NULL;
	void *agcHandle = NULL;	

	do 
	{
		WebRtcAgc_Create(&agcHandle);

		int minLevel = 0;
		int maxLevel = 255;
		int agcMode  = kAgcModeFixedDigital;
		WebRtcAgc_Init(agcHandle, minLevel, maxLevel, agcMode, fs);

		WebRtcAgc_config_t agcConfig;
		agcConfig.compressionGaindB = 20;
		agcConfig.limiterEnable     = 1;
		agcConfig.targetLevelDbfs   = 3;
		WebRtcAgc_set_config(agcHandle, agcConfig);

		infp = fopen(filename,"rb");
		int frameSize = 80;
		pData    = (short*)malloc(frameSize*sizeof(short));
		pOutData = (short*)malloc(frameSize*sizeof(short));

		outfp = fopen(outfilename,"wb");
		int len = frameSize*sizeof(short);
		int micLevelIn = 0;
		int micLevelOut = 0;
		while(TRUE)
		{
			memset(pData, 0, len);
			len = fread(pData, 1, len, infp);
			if (len > 0)
			{
				int inMicLevel  = micLevelOut;
				int outMicLevel = 0;
				uint8_t saturationWarning;
				int nAgcRet = WebRtcAgc_Process(agcHandle, pData, NULL, frameSize, pOutData,NULL, inMicLevel, &outMicLevel, 0, &saturationWarning);
				if (nAgcRet != 0)
				{
					printf("failed in WebRtcAgc_Process\n");
					break;
				}
				micLevelIn = outMicLevel;
				fwrite(pOutData, 1, len, outfp);
			}
			else
			{
				break;
			}
		}
	} while (0);

	fclose(infp);
	fclose(outfp);
	free(pData);
	free(pOutData);
	WebRtcAgc_Free(agcHandle);
}
*/


int main(int argc, char **argv)
{
	WebRtcAecTest();

	//WebRtcAgcTest("byby_8K_1C_16bit.pcm","byby_8K_1C_16bit_agc.pcm",8000);

	WebRtcNS32KSample("lhydd_1C_f32_32K.pcm","lhydd_1C_f32_32K_ns.pcm",32000,0);

	printf("声音增益，降噪结束...\n");

	//getchar();
	return 0;
}
