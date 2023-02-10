#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nvml.h>
#include <signal.h>
#include <sys/time.h>

#define GPU_NAME ""

const char* fmt = "%011ld.%06zd +%04ld.%06zd %010.6f\n";
#define fmtLen 43
#define bufEntries 1024
char buf[bufEntries*fmtLen+1];
char *bufPtr = buf;
struct timeval tv_last = {0};

char* bufPos()
{
    char* ret = bufPtr;
	bufPtr+=fmtLen;
	return ret;
}

int formatBuf(float value)
{
	struct timeval tv, diff;
	gettimeofday(&tv, NULL);
    timersub(&tv, &tv_last, &diff);
    if (diff.tv_sec > 9999) {
        diff.tv_sec = 9999;
        diff.tv_usec = 999999;
    }

	//printf(fmt, tv.tv_sec, tv.tv_usec, value);
	if (snprintf(bufPos(), fmtLen+1, fmt, tv.tv_sec, tv.tv_usec, diff.tv_sec, diff.tv_usec, value) != (int)fmtLen) {
	    printf("got %d instead of %d\n", snprintf(bufPtr, fmtLen+1, fmt, tv.tv_sec, tv.tv_usec, diff.tv_sec, diff.tv_usec, value), fmtLen);
		return 1;
	}
    tv_last = tv;
	return 0;
}

int printBuf(FILE* file)
{
	*bufPos() = '\0';
	fputs(buf, file);
	bufPtr = buf;
}

int bufFull()
{
   // printf("%p > %p\n", bufPtr,(buf+bufEntries*fmtLen));
    return bufPtr >= (buf+bufEntries*fmtLen);
}

void monitor_power(nvmlDevice_t device, FILE* outputFile)
{
    nvmlReturn_t result;
    unsigned int device_count, i;

    result = nvmlDeviceGetCount(&device_count);
    if (NVML_SUCCESS != result)
    { 
        printf("Failed to query device count: %s\n", nvmlErrorString(result));
        goto Error;
    }

    unsigned int power;

    result = nvmlDeviceGetPowerUsage(device, &power);

    if (NVML_ERROR_NOT_SUPPORTED == result)
        printf("This does not support power measurement\n");
    else if (NVML_SUCCESS != result)
    {
        printf("Failed to get power for device %i: %s\n", i, nvmlErrorString(result));
        goto Error;
    }
    if (formatBuf(power/1000.00) != 0) {
        fprintf(stderr, "error\n");
    }
    if (bufFull()) {
        printBuf(outputFile);
    }
    return;


 Error:
    result = nvmlShutdown();
    if (NVML_SUCCESS != result)
        printf("Failed to shutdown NVML: %s\n", nvmlErrorString(result));

    exit(1);
}

void shutdown_nvml()
{
    nvmlReturn_t result;
    result = nvmlShutdown();
    if (NVML_SUCCESS != result)
        printf("Failed to shutdown NVML: %s\n", nvmlErrorString(result));
}

void usage() {
    printf("Usage: binary <monitoring period (ms)>\n");
}

volatile sig_atomic_t flag = 0;

void end_monitoring(int sig) {
    flag = 1;
}

int main(int argc, char** argv) 
{
    if (argc != 3) {
        usage();
        return 0;
    }
    float sleep_useconds = 1.0/(atof(argv[1]))*1000000.00;
    int device_id = atoi(argv[2]);

    nvmlReturn_t result;
    unsigned int device_count, i;

    // First initialize NVML library
    result = nvmlInit();
    if (NVML_SUCCESS != result)
    { 
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));

        printf("Press ENTER to continue...\n");
        getchar();
        return 1;
    }


    result = nvmlDeviceGetCount(&device_count);
    if(NVML_SUCCESS != result)
    {
        printf("Failed to query device count: %s\n", nvmlErrorString(result));
        shutdown_nvml();
        exit(1);
    }

    printf("Found %d device%s\n\n", device_count, device_count != 1 ? "s" : "");


    nvmlDevice_t device;
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t pci;
    nvmlComputeMode_t compute_mode;
    unsigned int power;

    result = nvmlDeviceGetHandleByIndex(device_id, &device);
    if (NVML_SUCCESS != result)
    { 
        printf("Failed to get handle for device %i: %s\n", device_id, nvmlErrorString(result));
        exit(1);
    }

    result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
    if (NVML_SUCCESS != result)
    { 
        printf("Failed to get name of device %i: %s\n", 0, nvmlErrorString(result));
        exit(1);
    }

    signal(SIGINT, end_monitoring);

    do 
    {
        if (flag) {
            break;
        }
        monitor_power(device, stdout);
        //FILE* f = fopen("out.txt", "w");
        //monitor_power(device, f);
        //usleep(sleep_useconds);
    } while (1);

    shutdown_nvml();

    return 0;
}
