extern "C"{

    #include <pthread.h>
	#include <assert.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <sys/time.h>
	#include <semaphore.h>
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
#define RINGBUFFERSIZE 1024



using namespace std ;
using easywsclient::WebSocket;

static WebSocket::pointer wss = NULL;
static std::string host = "ws://localhost:8126/speaker";

void  * receiveData(void * argument);
void  * checkTime(void * argument);
void  * playData(void * argument);

void setAlsaVolume(long volume);



// declaration du semaphore
sem_t semaph;

// declaration de la fifo


//declaration mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_time = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cond2 = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_cond_t condition_var2 = PTHREAD_COND_INITIALIZER;

//definition de la structure d'une trame
typedef struct trame{
	char timeStamp[TIMESTAMPSIZE];
	long int  timeStamp_longint;
	unsigned char rootBuffer[ROOTBUFFERSIZE];
}trame;

queue<trame*> q;
long int msglobal_sum = 0;

//long int ms;
bool go = false;

//declaration d'un pointeur sur le haut de mon allocation de memoire


void handle_binaryMessage(const std::vector<uint8_t>& message)
{

	unsigned int iFrame;
	unsigned int nFrames;
	long int sum = 0;
	
	nFrames = message.size() / (ROOTBUFFERSIZE+TIMESTAMPSIZE);

	int aze = 0;
	trame * ptr;

	for (iFrame = 0; iFrame < nFrames; iFrame++) {

		ptr = (trame*) malloc(sizeof(trame));
		sum = 0;
		
		for (int j = 0; j < TIMESTAMPSIZE ; j++){
			ptr->timeStamp[j] = message[iFrame*(ROOTBUFFERSIZE+TIMESTAMPSIZE)+j];
			ptr->timeStamp[j] = ptr->timeStamp[j] & 0b00001111;
			sum *= 10;
			sum += ptr->timeStamp[j];
		}
		ptr->timeStamp_longint = sum;
		printf("sum = %lu\n", sum);

		if(iFrame == 0){
			msglobal_sum = sum;
			pthread_cond_signal( &condition_var );
		}


		//printf("timeStamp of frame %u = %lu\n", iFrame, ptr->timeStamp_longint);

		for (int k = 0; k < ROOTBUFFERSIZE; k++){
			ptr->rootBuffer[k] = message[iFrame*(ROOTBUFFERSIZE+TIMESTAMPSIZE)+TIMESTAMPSIZE+k];
		}

		// push pointer dans fifo
		pthread_mutex_lock( &mutex );
		q.push(ptr);
		pthread_mutex_unlock( &mutex );

		//printf("PUSH: %d: %p\n", iMessage, ptr);
		
	
		// semaphore --
		sem_post(&semaph);


	}

}


int main()
{

    pthread_t t_receiveData, t_checkTime, t_playData ; // declare threads.

    pthread_create( &t_receiveData, NULL, receiveData,NULL); // create a thread running receiveData
    pthread_create( &t_checkTime, NULL, checkTime, NULL); // create a thread running checkTime
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
    return 0;
}

void * checkTime(void * argument){
	
	pthread_cond_wait( &condition_var, &mutex_cond );

	struct timeval tp;
	long int ms = 0;
	while(1){
		gettimeofday(&tp, NULL);
		ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
		//printf("ms = %lu\n", ms);
		//printf("msglobal_sum = %lu\n", msglobal_sum);
		if (ms == msglobal_sum){
			printf("yolo les gars\n");
			
			//pthread_cond_signal( &condition_var2 );
		}
		usleep(245);
	}
		
	return 0;
}


void * playData(void * argument)
{


	trame * ptr;

	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	
	unsigned int val, val2;
	int dir;
    int val_list1;
	int rc;
	int size;
	char *buffer;



    // The "LISTING " displays some informations about current alsa version. 

	printf("##########\n##########LISTING 1##########\n####################\n");
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




	// The "LISTING 2" set the parameters of the PCM device
	printf("##########\n##########LISTING 2##########\n####################\n");





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

	/* Set period size to 1024 frames. */
	frames = ROOTBUFFERSIZE / 2;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}




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

	size = frames * 2;

	unsigned int popCounter = 0;
	
	bool goo = 1;
	struct timeval tp;
	long int ms = 0;

	setAlsaVolume(70);

	// waiting for condition_var2 signal to play 
	//pthread_cond_wait( &condition_var2, &mutex_cond2 );

	while(1){
		

		// semaphore --
		sem_wait(&semaph);
		
		// lock the mutex to get secure access to the queue
		pthread_mutex_lock( &mutex );
			ptr = q.front();
			//printf("POP: %d: %p\n", popCounter, ptr);
			q.pop();
		pthread_mutex_unlock( &mutex );	
		
		popCounter++;
	    
	    // allocate memory for the buffer that will be given to alsa
		buffer = (char *) malloc(sizeof(char) * size);

		// copy the data from websocket to a local buffer before to give it to asla
		memcpy(buffer, (ptr->rootBuffer), sizeof(char) * size);


		goo = 1;
		while(goo){
			gettimeofday(&tp, NULL);
			ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
			if (ms == ptr->timeStamp_longint){
				goo = 0;
			}
		}


		// write the buffer to alsa to be played
		rc = snd_pcm_writei(handle, buffer, frames);
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
	    free(buffer);	
	}

 	snd_pcm_drain(handle);
 	snd_pcm_close(handle);
 	return 0;
}


void setAlsaVolume(long volume)
{
	long min, max;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	const char *card = "default";
	const char *selem_name = "Master";

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, card);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_set_playback_volume_all(elem, volume * max/100);

	snd_mixer_close(handle);

}


