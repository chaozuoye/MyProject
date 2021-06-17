/*==============================================================================
�ڱ�������֮��ָ���ĵȴ�ʱ���ͬʱ������¼�ͻط���ͬ�����ݡ�
�����ͨ���ӳٿ�ʼ����ֱ����¼��ָ���ĺ�������ʵ�֡� 
������ʱ�������ٶȽ����б仯���Բ������Ż�¼�����������е��κ�Ư�ơ�
==============================================================================*/
#include "fmod.hpp"
#include "common.h"

#define LATENCY_MS      (50) /* һЩ�豸����Ҫ���ߵ��ӳ٣��Ա�����ֹ���*/
#define DRIFT_MS        (1)
#define DEVICE_INDEX    (0)
int FMOD_Main()
{
    int TYPE=0;
    int dspcount = 0;
    FMOD::Channel* channel = NULL;
    unsigned int samplesRecorded = 0;
    unsigned int samplesPlayed = 0;
    bool dspEnabled = false;

    void* extraDriverData = NULL;
    Common_Init(&extraDriverData);

    /*
        ����һ��System���󲢳�ʼ����
    */
    FMOD::System* system = NULL;
    FMOD_RESULT result = FMOD::System_Create(&system);
    FMOD::DSP* dsp;
    ERRCHECK(result);

    unsigned int version = 0;
    result = system->getVersion(&version);
    ERRCHECK(result);

    if (version < FMOD_VERSION)
    {
        Common_Fatal("FMOD lib version %08x doesn't match header version %08x", version, FMOD_VERSION);
    }

    result = system->init(100, FMOD_INIT_NORMAL, extraDriverData);
    ERRCHECK(result);

    int numDrivers = 0;
    result = system->getRecordNumDrivers(NULL, &numDrivers);
    ERRCHECK(result);

    if (numDrivers == 0)
    {
        Common_Fatal("No recording devices found/plugged in!  Aborting.");
    }

    /*
        ȷ�������е��ӳ١�
    */
    int nativeRate = 0;
    int nativeChannels = 0;
    result = system->getRecordDriverInfo(DEVICE_INDEX, NULL, 0, NULL, &nativeRate, NULL, &nativeChannels, NULL);
    ERRCHECK(result);

    unsigned int driftThreshold = (nativeRate * DRIFT_MS) / 1000;       /* The point where we start compensating for drift */
    unsigned int desiredLatency = (nativeRate * LATENCY_MS) / 1000;     /* �û�ָ���ӳ� */
    unsigned int adjustedLatency = desiredLatency;                      /*�û�ָ�����ӳٸ�������������¿����Ƚ��е��� */
    int actualLatency = desiredLatency;                                 /*�طſ�ʼ��������ӳ٣�����������*/

    /*
     ����Ҫ¼�Ƶ��û�������Ȼ��ʼ¼�ơ�
    */
    FMOD_CREATESOUNDEXINFO exinfo = { 0 };                  //����һ��FMOD_CREATESOUNDEXINFO�ṹ��, �����������ݵĳ���.
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO); //����ṹ�Ĵ�С
    exinfo.numchannels = nativeChannels;    //FMOD_OPENUSER / FMOD_OPENRAW�����������е�ͨ������
    exinfo.format = FMOD_SOUND_FORMAT_PCM16;    //FMOD_OPENUSER / FMOD_OPENRAW���������ݸ�ʽ��(fmod_sound_format)
    exinfo.defaultfrequency = nativeRate;   //FMOD_OPENUSER / FMOD_OPENRAW���������ݵ�Ĭ��Ƶ�ʡ�
    exinfo.length = nativeRate * sizeof(short) * nativeChannels; /* 1 second buffer, size here doesn't change latency */

    FMOD::Sound* sound = NULL;
    result = system->createSound(0, FMOD_LOOP_NORMAL | FMOD_OPENUSER, &exinfo, &sound);
    ERRCHECK(result);

    result = system->recordStart(DEVICE_INDEX, sound, true);
    ERRCHECK(result);

    unsigned int soundLength = 0;
    result = sound->getLength(&soundLength, FMOD_TIMEUNIT_PCM);
    ERRCHECK(result);

    /*
        Main loop
    */
    do
    {
        Common_Update();
        /*
            ȷ�����ϴμ�������Ѽ�¼�˶�������
        */
        unsigned int recordPos = 0;
        result = system->getRecordPosition(DEVICE_INDEX, &recordPos);   //����PCM�����м�¼�������ĵ�ǰ��¼λ�á�recordPos��ǰ��¼λ��
        if (result != FMOD_ERR_RECORD_DISCONNECTED) //FMOD_ERR_RECORD_DISCONNECTED��ʾָ����¼�����������ѶϿ����ӡ�
        {
            ERRCHECK(result);
        }

        static unsigned int lastRecordPos = 0;
        unsigned int recordDelta = (recordPos >= lastRecordPos) ? (recordPos - lastRecordPos) : (recordPos + soundLength - lastRecordPos);
        lastRecordPos = recordPos;
        samplesRecorded += recordDelta;

        static unsigned int minRecordDelta = (unsigned int)-1;
        if (recordDelta && (recordDelta < minRecordDelta))
        {
            minRecordDelta = recordDelta; /* Smallest driver granularity seen so far */
            adjustedLatency = (recordDelta <= desiredLatency) ? desiredLatency : recordDelta; /* Adjust our latency if driver granularity is high */
        }

        /*

            �ӳٲ��ţ�ֱ���ﵽ�����������ӳ١�
        */
        if (!channel && samplesRecorded >= adjustedLatency)
        {
            result = system->playSound(sound, 0, false, &channel);
            ERRCHECK(result);
        }
        if (Common_BtnPress(BTN_ACTION1)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            TYPE = 0;
        }
        else if (Common_BtnPress(BTN_ACTION2)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp);    // �ɸı�����
            dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_PITCH, 8.0);     // 8.0 Ϊһ���˶�
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 1;
        }
        else if (Common_BtnPress(BTN_ACTION3)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp);
            dsp->setParameterFloat(FMOD_DSP_PITCHSHIFT_PITCH, 0.8);
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 2;
        }
        else if (Common_BtnPress(BTN_ACTION4)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_FLANGE, &dsp);       
            dsp->setParameterFloat(FMOD_DSP_FLANGE_RATE, 10);           
            dsp->setParameterFloat(FMOD_DSP_FLANGE_MIX, 70);
            dsp->setParameterFloat(FMOD_DSP_FLANGE_DEPTH, 0.4);
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 3;
        }
        else if (Common_BtnPress(BTN_ACTION5)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            float frequency;
            system->createDSPByType(FMOD_DSP_TYPE_NORMALIZE, &dsp);    //�Ŵ�����
            channel->addDSP(0, dsp);

            channel->getFrequency(&frequency);
            frequency = frequency * 2;                                  //Ƶ��*2
            channel->setFrequency(frequency);
            dspcount++;
            TYPE = 4;
        }
        else if (Common_BtnPress(BTN_ACTION6)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_ECHO, &dsp);          // ���ƻ���
            dsp->setParameterFloat(FMOD_DSP_ECHO_DELAY, 300);           // ��ʱ
            dsp->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK, 20);         // �ز�˥�����ӳ�
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 5;
        }
        else if (Common_BtnPress(BTN_ACTION7)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_ECHO, &dsp);
            dsp->setParameterFloat(FMOD_DSP_ECHO_DELAY, 100);
            dsp->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK, 50);
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 6;
        }
        else if (Common_BtnPress(BTN_ACTION8)) {
            if (dspcount != 0)
            {
                channel->removeDSP(dsp);
                dspcount--;
            }
            system->createDSPByType(FMOD_DSP_TYPE_TREMOLO, &dsp);
            dsp->setParameterFloat(FMOD_DSP_TREMOLO_SKEW, 0.8);
            channel->addDSP(0, dsp);
            dspcount++;
            TYPE = 7;
        }
        if (channel)
        {
            /*
               ���¼��ֹͣ�򲥷�
            */
            bool isRecording = false;
            result = system->isRecording(DEVICE_INDEX, &isRecording);
            if (result != FMOD_ERR_RECORD_DISCONNECTED)
            {
                ERRCHECK(result);
            }

            if (!isRecording)
            {
                result = channel->setPaused(true);
                ERRCHECK(result);
            }

            /*
                Determine how much has been played since we last checked.
            */
            unsigned int playPos = 0;
            result = channel->getPosition(&playPos, FMOD_TIMEUNIT_PCM);
            ERRCHECK(result);

            static unsigned int lastPlayPos = 0;
            unsigned int playDelta = (playPos >= lastPlayPos) ? (playPos - lastPlayPos) : (playPos + soundLength - lastPlayPos);
            lastPlayPos = playPos;
            samplesPlayed += playDelta;

            /*
                Compensate for any drift.
            */
            int latency = samplesRecorded - samplesPlayed;
            actualLatency = (int)((0.97f * actualLatency) + (0.03f * latency));

            int playbackRate = nativeRate;
            if (actualLatency < (int)(adjustedLatency - driftThreshold))
            {
                /* Play position is catching up to the record position, slow playback down by 2% */
                playbackRate = nativeRate - (nativeRate / 50);
            }
            else if (actualLatency > (int)(adjustedLatency + driftThreshold))
            {
                /* Play position is falling behind the record position, speed playback up by 2% */
                playbackRate = nativeRate + (nativeRate / 50);
            }

            channel->setFrequency((float)playbackRate);
            ERRCHECK(result);
        }

        Common_Draw("==================================================");
        Common_Draw("Record Example.");
        Common_Draw("Copyright (c) Firelight Technologies 2004-2021.");
        Common_Draw("==================================================");
        Common_Draw("");
        Common_Draw("Adjust LATENCY define to compensate for stuttering");
        Common_Draw("Current value is %dms", LATENCY_MS);
        Common_Draw("");
        Common_Draw("1---------------------TYPE_NORMAL");
        Common_Draw("2---------------------TYPE_LOLITA");
        Common_Draw("3---------------------TYPE_UNCLE");
        Common_Draw("4---------------------TYPE_THRILLER");
        Common_Draw("5---------------------TYPE_FUNNY");
        Common_Draw("6---------------------TYPE_ETHEREAL");
        Common_Draw("7---------------------TYPE_CHORUS");
        Common_Draw("8---------------------TYPE_TREMOLO");
        Common_Draw("Press %s to quit", Common_BtnStr(BTN_QUIT));
        Common_Draw("");
        Common_Draw("Adjusted latency: %4d (%dms)", adjustedLatency, adjustedLatency * 1000 / nativeRate);
        Common_Draw("Actual latency:   %4d (%dms)", actualLatency, actualLatency * 1000 / nativeRate);
        Common_Draw("");
        Common_Draw("Recorded: %5d (%ds)", samplesRecorded, samplesRecorded / nativeRate);
        Common_Draw("Played:   %5d (%ds)", samplesPlayed, samplesPlayed / nativeRate);
        Common_Draw("botton:   %5d ",(TYPE+1));

        Common_Sleep(10);
    } while (!Common_BtnPress(BTN_QUIT));

    /*
        Shut down
    */
    result = sound->release();
    ERRCHECK(result);
    result = system->release();
    ERRCHECK(result);

    Common_Close();

    return 0;
}
