// Author: Sergey Chaban <sergey.chaban@gmail.com>

struct TSK_SIGNAL;
struct TSK_WORKER;
struct TSK_BRIGADE;
struct TSK_QUEUE;
struct TSK_JOB;
struct TSK_CONTEXT;

typedef void(*TSK_WRK_FUNC)(void*);
typedef void(*TSK_JOB_FUNC)(TSK_CONTEXT*);

struct TSK_JOB {
	TSK_JOB_FUNC mFunc;
	void* mpData;
	uint8_t mState[4];
	uint16_t mCouter;
};

struct TSK_CONTEXT {
	TSK_JOB* mpJob;
	TSK_BRIGADE* mpBrigade;
	int mWrkId;
	int mJobCount;
};

TSK_SIGNAL* tskSignalCreate();
void tskSignalDestroy(TSK_SIGNAL* pSig);
bool tskSignalWait(TSK_SIGNAL* pSig);
bool tskSignalSet(TSK_SIGNAL* pSig);
bool tskSignalReset(TSK_SIGNAL* pSig);

TSK_WORKER* tskWorkerCreate(TSK_WRK_FUNC func, void* pData);
void tskWorkerDestroy(TSK_WORKER* pWrk);
void tskWorkerExec(TSK_WORKER* pWrk);
void tskWorkerWait(TSK_WORKER* pWrk);
void tskWorkerStop(TSK_WORKER* pWrk);

TSK_BRIGADE* tskBrigadeCreate(int wrkNum);
void tskBrigadeDestroy(TSK_BRIGADE* pBgd);
void tskBrigadeExec(TSK_BRIGADE* pBgd, TSK_QUEUE* pQue);
void tskBrigadeWait(TSK_BRIGADE* pBgd);
bool tskBrigadeCkWrkId(TSK_BRIGADE* pBgd, int wrkId);
TSK_CONTEXT* tskBrigadeGetContext(TSK_BRIGADE* pBgd, int wrkId);

TSK_QUEUE* tskQueueCreate(int nslots);
void tskQueueDestroy(TSK_QUEUE* pQue);
void tskQueueAdd(TSK_QUEUE* pQue, TSK_JOB* pJob);
void tskQueuePurge(TSK_QUEUE* pQue);
void tskQueueExec(TSK_QUEUE* pQue, TSK_BRIGADE* pBgd);
int tskQueueJobsCount(TSK_QUEUE* pQue);


