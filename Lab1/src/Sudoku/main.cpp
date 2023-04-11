#include "sudoku.hpp"
using namespace std;

#define IF_DEBUG 0
#define MAX_PRO 133331
#define MAX_FILENAEM_LEN 128
#define MAX_LEN 100
#define SOLVER_MAX 32
int numOfWorkerThread=get_nprocs_conf()+1;
int workload[SOLVER_MAX];
int boards[SOLVER_MAX][81];
long int problemnum,solvenum,dispatchnum,printnum,getnum;
long int start;
bool noinput=false;
//-------------mutex define--------------

pthread_mutex_t jobqueue_mutex;
pthread_mutex_t resultqueue_mutex;
pthread_mutex_t use_mutex;
pthread_cond_t result_cndFull;
pthread_cond_t result_cndEmpty;
pthread_cond_t use_cndFull;
pthread_mutex_t workqueue_mutex[SOLVER_MAX];
pthread_cond_t workqueue_cndFull[SOLVER_MAX];
pthread_cond_t workqueue_cndEmpty[SOLVER_MAX];
typedef struct {
  char *job;
  long int jobID;
} Job;

//-------------queue define-----------------
queue<Job*> JobQueue ;
queue<Job*> workqueue[SOLVER_MAX];
Job* id2point[MAX_PRO];
bool st[MAX_PRO];
bool use[MAX_PRO];

void * readfunc(void * args)
{
  char filename[MAX_FILENAEM_LEN];
  long int nextJobID=0;
  
  while(cin.getline(filename,MAX_FILENAEM_LEN))
  {
    char line[MAX_LEN];
    FILE *fp;
    fp = fopen(filename, "r"); 
    if (fp == NULL) {
        printf("Failed to open file.\n");
        exit(1);
    }
    while (fgets(line, sizeof line, fp) != NULL) {
      //printf("%d\n",strlen(line));
      if(strlen(line)>=81){
          getnum++;
          Job * pJob=(Job*)malloc(sizeof(Job)); 
          pJob->job=(char*)malloc(MAX_LEN*sizeof(pJob->job[0]));
          pJob->jobID=nextJobID;
          strcpy(pJob->job,line);

          pthread_mutex_lock(&use_mutex);
          while(use[pJob->jobID])
          {
            pthread_cond_wait(&use_cndFull,&use_mutex);
          }
          pthread_mutex_unlock(&use_mutex);

          pthread_mutex_lock(&jobqueue_mutex);
          JobQueue.push(pJob);
          problemnum++;
          use[pJob->jobID]=true;
          nextJobID=(nextJobID+1)%MAX_PRO;
          pthread_mutex_unlock(&jobqueue_mutex);
        }
        }
      fclose(fp);
  }
    noinput=true;
  return NULL;
}


void* dispatchfunc(void *args)
{
  while(1)
  {
    pthread_mutex_lock(&jobqueue_mutex);
    if(dispatchnum==problemnum&&noinput){
      for(int i=0;i<numOfWorkerThread;i++) 
        pthread_cond_broadcast(&workqueue_cndEmpty[i]);
      pthread_mutex_unlock(&jobqueue_mutex);
      break;
    }
    if(JobQueue.empty())
    {
      pthread_mutex_unlock(&jobqueue_mutex);
      usleep(10000);
      continue;
    }
    Job*t=JobQueue.front();
    JobQueue.pop();
    pthread_mutex_unlock(&jobqueue_mutex);
    int id=0;
      for(int i=0;i<numOfWorkerThread;i++)
      {
        if(workload[i]<workload[id])
        {
          id=i;
        }
      }
      pthread_mutex_lock(&workqueue_mutex[id]);
      workqueue[id].push(t);
      workload[id]++;
      dispatchnum++;
      pthread_cond_broadcast(&workqueue_cndEmpty[id]);
      pthread_mutex_unlock(&workqueue_mutex[id]);
    
  }
return NULL;
}
void * workerthread(void *args)
{
  int id=*((int*)args);
  Job*problem;
  while(1)
  {
    pthread_mutex_lock(&workqueue_mutex[id]);
    while(workqueue[id].empty())
    {
      if(dispatchnum==problemnum&&noinput)
      {
        pthread_cond_broadcast(&result_cndEmpty);
        pthread_mutex_unlock(&workqueue_mutex[id]);
        pthread_exit(NULL);
      }
      pthread_cond_wait(&workqueue_cndEmpty[id],&workqueue_mutex[id]);
    }
    problem=workqueue[id].front();
    workqueue[id].pop();
    workload[id]--;
    pthread_mutex_unlock(&workqueue_mutex[id]);
    solver(problem->job,boards[id]);

    pthread_mutex_lock(&resultqueue_mutex);
    if(problemnum==solvenum&&noinput)
    {
      pthread_cond_broadcast(&result_cndEmpty);
      pthread_mutex_unlock(&resultqueue_mutex);
      break;
    }
    solvenum++;
    id2point[problem->jobID%MAX_PRO]=problem;
    st[problem->jobID%MAX_PRO]=true;
    pthread_cond_broadcast(&result_cndEmpty);
    pthread_mutex_unlock(&resultqueue_mutex);
  }
  return NULL;
}
void*printthread(void*args)
{
    while(1)
    {
      pthread_mutex_lock(&resultqueue_mutex);
      while(!st[start])
      {
        if(printnum==problemnum&&noinput)
        {
          pthread_cond_broadcast(&result_cndFull);
          pthread_mutex_unlock(&resultqueue_mutex);
          pthread_exit(NULL);        
        }
        pthread_cond_wait(&result_cndEmpty,&resultqueue_mutex);
      }
      id2point[start]->job[81]='\0';
      printf("%s\n",id2point[start]->job);
      st[start]=false;

      pthread_mutex_lock(&use_mutex);
      use[id2point[start]->jobID]=false;
      pthread_cond_broadcast(&use_cndFull);
      pthread_mutex_unlock(&use_mutex);

      free(id2point[start]->job);
      free(id2point[start]);
      start=(start+1)%MAX_PRO;
      printnum++;
      pthread_cond_broadcast(&result_cndFull);
      pthread_mutex_unlock(&resultqueue_mutex);
    }
  return NULL;
}
int main(int argc, char *argv[])
{
  pthread_mutex_init(&jobqueue_mutex,NULL);
  pthread_mutex_init(&resultqueue_mutex,NULL);
  pthread_mutex_init(&use_mutex,NULL);
  pthread_cond_init(&result_cndFull,NULL);
  pthread_cond_init(&result_cndEmpty,NULL);
  pthread_cond_init(&use_cndFull,NULL);
  for(int i=0;i<numOfWorkerThread;i++)
  {
    pthread_cond_init(&workqueue_cndFull[i],NULL);
    pthread_cond_init(&workqueue_cndEmpty[i],NULL);
    pthread_mutex_init(&workqueue_mutex[i],NULL);
  }
  pthread_t reader;
  if(pthread_create(&reader, NULL, readfunc, NULL)!=0)
  {
      perror("pthread_create failed");
      exit(1);
  }

  pthread_t dispatch;
  if(pthread_create(&dispatch, NULL, dispatchfunc, NULL)!=0)
  {
      perror("pthread_create failed");
      exit(1);
  }
  pthread_t printer;
  if(pthread_create(&printer, NULL, printthread, NULL)!=0)
  {
      perror("pthread_create failed");
      exit(1);
  }
  pthread_t worker[numOfWorkerThread];
  int workerID[numOfWorkerThread];
  for(int i=0;i<numOfWorkerThread;i++)
  {
    workerID[i]=i;
    if(pthread_create(&worker[i], NULL, workerthread, &workerID[i])!=0)
    {
      perror("pthread_create failed");
      exit(1);
    }
  }
  pthread_join(reader,NULL);
  pthread_join(dispatch,NULL);
  pthread_join(printer,NULL);
  for(int i=0;i<numOfWorkerThread;i++) pthread_join(worker[i],NULL);
  exit(0);
}
