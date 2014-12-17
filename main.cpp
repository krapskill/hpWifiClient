extern "C"{

    #include <pthread.h>
	#include <assert.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <sys/time.h>
	#include <semaphore.h>
	#include <math.h>
}

#include "easywsclient.hpp"
#include <iostream>
#include <fstream>

#include <vector>
#include <queue>
#include <string>
#include <sstream>
#include <alsa/asoundlib.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define TIMESTAMPSIZE 13
#define ROOTBUFFERSIZE 2048
#define RINGBUFFERSIZE ROOTBUFFERSIZE*50




using namespace std ;
using easywsclient::WebSocket;

static WebSocket::pointer wss = NULL;
static std::string host = "";

void  * receiveData(void * argument);
void  * checkTime(void * argument);
void  * playData(void * argument);

void showCurrentAlsaInfo(void);
void showGlobalAlsaInfo(void);
void initAlsa(void);
void bufferWrite(const char * frameSrc);
void askingForIPAdress(void);


snd_pcm_uframes_t frames = ROOTBUFFERSIZE / 2;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
int rc;
int dir;
unsigned int val, val2;

// declaration du semaphore
sem_t semaph;
sem_t semaphinit;

	 char ringBuffer[RINGBUFFERSIZE];

	unsigned int iRead = 0;
	unsigned int iWrite = 0;

	int ringBufferCounter = 0;
// FLAGS
bool stopFlag = 1;
bool startFlag = 0;

//declaration mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_time = PTHREAD_MUTEX_INITIALIZER;

//mutex pour les signaux
pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cond2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cond3 = PTHREAD_MUTEX_INITIALIZER;

//signaux
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_cond_t condition_var2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t condition_var3 = PTHREAD_COND_INITIALIZER;



//definition de la structure d'une trame
typedef struct trame{
	int trameNumber;
	char timeStamp[TIMESTAMPSIZE];
	long long timeStamp_longint;
	 char rootBuffer[ROOTBUFFERSIZE];
}trame;

queue<trame*> q;

long long msglobal_sum = 0;


//declaration d'un pointeur sur le haut de mon allocation de memoire


void handle_binaryMessage(const std::vector<uint8_t>& message)
{

	unsigned int iFrame;
	unsigned int nFrames;
	long long sum = 0;
	
	nFrames = message.size() / (ROOTBUFFERSIZE+TIMESTAMPSIZE);

	pthread_t t_checkTime;
	pthread_create( &t_checkTime, NULL, checkTime, NULL); // create a thread running checkTime


	trame * ptr;

	std::cout << "message  size = " << message.size() / (ROOTBUFFERSIZE+TIMESTAMPSIZE) << '\n';

	if (message[0] == 9){
		startFlag = 0;
		for (int i = 0 ; i < q.size() ; i++){
			q.pop();
		}
		//semaph = semaphinit;
		printf("emptying queue\n");
		//snd_pcm_drain(handle);
		snd_pcm_drop(handle);
		sem_init(&semaph, 1,0);
	}else{

		printf("Data received\n");
		for (iFrame = 0; iFrame < nFrames; iFrame++) {

			ptr = (trame*) malloc(sizeof(trame));
		//	ptrRingBuffer = (ringBuffer*) malloc(sizeof(ringBuffer));
			sum = 0;
			
			for (int j = 0; j < TIMESTAMPSIZE ; j++){
				ptr->timeStamp[j] = message[iFrame*(ROOTBUFFERSIZE+TIMESTAMPSIZE)+j];
				ptr->timeStamp[j] = ptr->timeStamp[j] & 0b00001111;
				sum *= 10;
				sum += ptr->timeStamp[j];
			//	printf("sum temp = %llu\n", sum);
			}
			ptr->timeStamp_longint = sum;
			//printf("sum = %llu\n", sum);
			//printf("ptr timestamp = %llu\n", ptr->timeStamp_longint);

			if(iFrame == 0){
				msglobal_sum = sum;
				printf("Storing datas\n");
				printf("First timestamp = %lld\n", msglobal_sum);
				pthread_cond_signal( &condition_var );
			}


			for (int k = 0; k < ROOTBUFFERSIZE; k++){
				ptr->rootBuffer[k] = message[iFrame*(ROOTBUFFERSIZE+TIMESTAMPSIZE)+TIMESTAMPSIZE+k];
				//ptrRingBuffer->buffer[k] = ptr->rootBuffer[k];
				//testBuffer[k] = ptr->rootBuffer[k];
			}
			ptr->trameNumber = iFrame;
			// push pointer dans fifo
			pthread_mutex_lock( &mutex );
			if (iFrame < 20){
				bufferWrite(ptr->rootBuffer);
			}
			q.push(ptr);
			//qRing.push(ptrRingBuffer);
			pthread_mutex_unlock( &mutex );

			//printf("PUSH: %d: %p\n", iMessage, ptr);
			
			// semaphore --
			sem_post(&semaph);
		}
	}
	printf("Datas stored \n");
}


int main(int argc, char *argv[])
{
 

 	askingForIPAdress();


    pthread_t t_receiveData, t_checkTime, t_playData ; // declare threads.

    pthread_create( &t_receiveData, NULL, receiveData,NULL); // create a thread running receiveData
   // pthread_create( &t_checkTime, NULL, checkTime, NULL); // create a thread running checkTime
    pthread_create( &t_playData, NULL, playData,NULL); // create a thread running playData
 
	// The pthread_create() call :
	// int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
	// Parameters :
	// thread : The unique identifier for the thread. This identifier has to be of type pthread_t.
	// attr : This is an object which you can create for the thread with specific attributes for the thread. Can be NULL if you want to use the default attributes. The attributes will not be handled in this tutorial.
	// start_routine : The function that the thread has to execute.
	// arg : The function argument. If you don't want to pass an argument, set it to NULL
	// returns : 0 on success, some error code on failure. 
	
    // Because all created threads are terminated when main() finishes, we have
    // to give the threads some time to finish. Unfortunately for function1, main()
    // will give only 1 second, but function1 needs at least 2 seconds. So function1 will
    // probably be terminated before it can finish. This is a BAD way to manage threads.

    pthread_join(t_receiveData, NULL);
    pthread_join(t_checkTime, NULL);
    pthread_join(t_playData, NULL);

    return 0;
}


void * receiveData(void * argument)
{
    wss = WebSocket::from_url(host);
	assert(wss);
	std::ostringstream os;
    
	while (wss->getReadyState() != WebSocket::CLOSED) {
		wss->poll();
		wss->dispatchBinary(handle_binaryMessage);
	}
	
	delete wss;
	printf("websocket thread end\n");
    return 0;
}

void * checkTime(void * argument){
	
	pthread_cond_wait( &condition_var, &mutex_cond );

	struct timeval tp;
	long long ms = 0;
	while(1){
		gettimeofday(&tp, NULL);
		ms = (tp.tv_sec * 1000LL) + (tp.tv_usec / 1000) ;
		if (ms == msglobal_sum){
			printf("Time to play\n");
			//stopFlag = 0;
			startFlag = 1;
			//pthread_cond_signal (&condition_var2);
			// go out of the while(1) if it is time to play
			break;
		}
	}

	printf("end of checktime thread\n");	
	return 0;
}


void * playData(void * argument)
{

printf("Start initialising Alsa\n");
initAlsa();
printf("end of initialising Alsa\n");

	trame * ptr;

	int size;
	char *buffer;

	size = frames * 2;

	unsigned int popCounter = 0;
	
	float k = 1;
	float count = 0.0;
	struct timeval tp;
	long long ms = 0;



	while(1){
		// waiting for condition_var2 signal to play 
		//pthread_cond_wait( &condition_var2, &mutex_cond2 );
		while(startFlag){	
			// semaphore --
			sem_wait(&semaph);
			
			
			// lock the mutex to get secure access to the queue
			pthread_mutex_lock( &mutex );
				ptr = q.front();
				//ptrRingBuffer = qRing.front();
				//printf("POP: %d: %p\n", popCounter, ptr);
				q.pop();
				//qRing.pop();
				if ( ringBufferCounter < 20){
					bufferWrite(ptr->rootBuffer);
					
				}
			pthread_mutex_unlock( &mutex );	
			
			//popCounter++;
		    
		    // allocate memory for the buffer that will be given to alsa
			buffer = (char *) malloc(sizeof(char) * size);
			
			// copy the data from websocket to a local buffer before t o give it to asla
			//memcpy(buffer, (ptr->rootBuffer), sizeof(char) * size);  

	 
			gettimeofday(&tp, NULL);
			ms = ((tp.tv_sec)*1000LL) + ((tp.tv_usec)/1000);

			if(ms > ptr->timeStamp_longint){
				k = 0.99;
			}else if(ms < ptr->timeStamp_longint){
				k = 1.01;
			}else {
				k = 1;
			}
			
			ringBufferCounter--;
			//printf("ringBufferCounter = %u\n", ringBufferCounter);

			for (int iFrame = 0 ; iFrame < 2048 ; iFrame+=2){
				
				float intPart = floor(count);
				
				float fracPart = count - intPart;
				
				unsigned int x0 = (unsigned int)intPart;
				unsigned int x1 = x0 + 1;
				if(x1 == RINGBUFFERSIZE){
					x1 = 0;
				}
				
				char y0_lsb = ringBuffer[2*x0];
				char y0_msb = ringBuffer[2*x0 + 1];
				char y1_lsb = ringBuffer[2*x1];
				char y1_msb = ringBuffer[2*x1 + 1];
				
				short y0s = (((short)y0_msb) << 8) + (short)y0_lsb;
				short y1s = (((short)y1_msb) << 8) + (short)y1_lsb;
				
				float y0 = (float)y0s;
				float y1 = (float)y1s;

				float y = y0 + ((y1-y0)*(count-intPart));

				short ys = (short)y;

				char y_msb = (char)(ys >> 8);
				char y_lsb = (char)(ys & 0x00FF);

				ptr->rootBuffer[iFrame] = y_lsb;
				ptr->rootBuffer[iFrame +1] = y_msb;
				
				count += k;
				if(count >= RINGBUFFERSIZE/2){
					count -= RINGBUFFERSIZE/2;
				}
			}

			memcpy(buffer, ptr->rootBuffer, sizeof(char) * size);
			rc = snd_pcm_writei(handle, buffer, frames);

			if (ptr->trameNumber == 0){
				printf("start playing\n");
			}

			// write the buffer to alsa to be played
			
			
		/*
		    if (rc == -EPIPE) {
		      // EPIPE means underrun //
		      fprintf(stderr, "underrun occurred");
		      snd_pcm_prepare(handle);
		    } else if (rc < 0) {
		      fprintf(stderr, "error from writei: %s",
		              snd_strerror(rc));
		    }  else if (rc != (int)frames) {
		      fprintf(stderr, "short write, write %d frames", rc);
		    }  else if (rc == -ESTRPIPE) {
		      fprintf(stderr, "ESTRPIPE bro !! %d ", rc);
		    }  else if (rc == -EBADFD) {
		      fprintf(stderr, "EBADFD bro !! %d ", rc);
		    }  else  {
		      //fprintf(stderr, "yolo2 %d ", rc);
		    }	
		*/
		    // free the pointer and buffer to get enough memory
		    free(ptr);
		    //free(ptrRingBuffer);
		    free(buffer);	
		}
	}

 	snd_pcm_drain(handle);
 	snd_pcm_close(handle);
 	return 0;
}


void bufferWrite(const char * frameSrc){
	for(int iFrame = 0 ; iFrame < 2048 ; iFrame+=2){
		ringBuffer[iWrite] = frameSrc[iFrame];
		ringBuffer[iWrite + 1] = frameSrc[iFrame + 1];
		iWrite+=2;
		if (iWrite == RINGBUFFERSIZE){
			iWrite = 0;
		}
	}
	ringBufferCounter++;
}

void askingForIPAdress(void){
	char a []= "ws://";
 	char b []= ":8126/speaker";
 	char str [15];

	printf ("Enter your family name: ");
  	scanf ("%15s",str);
  	
  	char full_name[33];
	strcpy(full_name, a);
	strcat(full_name, str);
	strcat(full_name, b);

	printf("host = %s\n", full_name);
	host = full_name;


}

void initAlsa(void){

	/* Open PCM device for playback. */
	rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	/* One channel mode (mono) */
	snd_pcm_hw_params_set_channels(handle, params, 1);

	/* 48000 bits/second sampling rate  */
	val = 48000;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);


	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}


}

void showGlobalAlsaInfo (void){

	int val_list1;

    // The "LISTING 1" displays some informations about current alsa version. 

	printf("##########\n########## ALSA GLOBAL INFORMATIONS ##########\n####################\n");
	// Display the ALSA library version
	printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

	// Display the stream types supported by ALSA
	printf("\nPCM stream types:\n");
	for (val_list1 = 0; val_list1 <= SND_PCM_STREAM_LAST; val_list1++){
		printf("  %s\n", snd_pcm_stream_name((snd_pcm_stream_t)val_list1));
	}

	// Display the access types supported by ALSA
	printf("\nPCM access types:\n");
	for (val_list1 = 0; val_list1 <= SND_PCM_ACCESS_LAST; val_list1++){
		printf("  %s\n", snd_pcm_access_name((snd_pcm_access_t)val_list1));
	}

	// Display the formats supported by ALSA
	printf("\nPCM formats:\n");
	for (val_list1 = 0; val_list1 <= SND_PCM_FORMAT_LAST; val_list1++){
		if (snd_pcm_format_name((snd_pcm_format_t)val_list1) != NULL)
	  printf("  %s (%s)\n", snd_pcm_format_name((snd_pcm_format_t)val_list1), snd_pcm_format_description((snd_pcm_format_t)val_list1));

	}

	// Display the subformats supported by ALSA
	printf("\nPCM subformats:\n");
	for (val_list1 = 0; val_list1 <= SND_PCM_SUBFORMAT_LAST;val_list1++){
		printf("  %s (%s)\n", snd_pcm_subformat_name((snd_pcm_subformat_t)val_list1), snd_pcm_subformat_description((snd_pcm_subformat_t)val_list1));
	}

	// Display the available states
	printf("\nPCM states:\n");
	for (val_list1 = 0; val_list1 <= SND_PCM_STATE_LAST; val_list1++){
		printf("  %s\n", snd_pcm_state_name((snd_pcm_state_t)val_list1));
	}
}




void showCurrentAlsaInfo(void){

	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	
	int rc;
	int dir;
	unsigned int val, val2;


	/* Display information about the PCM interface as set up before*/

	printf("PCM handle name = '%s'\n", snd_pcm_name(handle));

	printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(handle)));

	snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
	printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

	snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *) &val);
	printf("format = '%s' (%s)\n", snd_pcm_format_name((snd_pcm_format_t)val), snd_pcm_format_description((snd_pcm_format_t)val));

	snd_pcm_hw_params_get_subformat(params, (snd_pcm_subformat_t *)&val);
	printf("subformat = '%s' (%s)\n", snd_pcm_subformat_name((snd_pcm_subformat_t)val),	snd_pcm_subformat_description((snd_pcm_subformat_t)val));

	snd_pcm_hw_params_get_channels(params, &val);
	printf("channels = %d\n", val);

	snd_pcm_hw_params_get_rate(params, &val, &dir);
	printf("rate = %d bps\n", val);

	snd_pcm_hw_params_get_period_time(params, &val, &dir);
	printf("period time = %d us\n", val);

	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	printf("period size = %d frames\n", (int)frames);

	snd_pcm_hw_params_get_buffer_time(params, &val, &dir);
	printf("buffer time = %d us\n", val);

	snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);
	printf("buffer size = %d frames\n", val);

	snd_pcm_hw_params_get_periods(params, &val, &dir);
	printf("periods per buffer = %d frames\n", val);

	snd_pcm_hw_params_get_rate_numden(params, &val, &val2);
	printf("exact rate = %d/%d bps\n", val, val2);

	val = snd_pcm_hw_params_get_sbits(params);
	printf("significant bits = %d\n", val);

	val = snd_pcm_hw_params_is_batch(params);
	printf("is batch = %d\n", val);

	val = snd_pcm_hw_params_is_block_transfer(params);
	printf("is block transfer = %d\n", val);

	val = snd_pcm_hw_params_is_double(params);
	printf("is double = %d\n", val);

	val = snd_pcm_hw_params_is_half_duplex(params);
	printf("is half duplex = %d\n", val);

	val = snd_pcm_hw_params_is_joint_duplex(params);
	printf("is joint duplex = %d\n", val);

	val = snd_pcm_hw_params_can_overrange(params);
	printf("can overrange = %d\n", val);

	val = snd_pcm_hw_params_can_mmap_sample_resolution(params);
	printf("can mmap = %d\n", val);

	val = snd_pcm_hw_params_can_pause(params);
	printf("can pause = %d\n", val);

	val = snd_pcm_hw_params_can_resume(params);
	printf("can resume = %d\n", val);

	val = snd_pcm_hw_params_can_sync_start(params);
	printf("can sync start = %d\n", val);


}


