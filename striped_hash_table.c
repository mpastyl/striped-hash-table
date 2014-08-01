#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"

// node of a list (bucket)
struct node_t{
    int value;
    int hash_code;
    struct node_t * next;
};


struct HashSet{
    //int length;
    struct node_t ** table;
    int capacity;
    int setSize;
    int * locks;
    int locks_length;
};

void lock_set (struct HashSet * H, int hash_code){

    
    int indx=hash_code % H->locks_length;
    while (1){
        if (!H->locks[indx]){
            if(!__sync_lock_test_and_set(&(H->locks[indx]),1)) break;
        }
    }
}

void unlock_set(struct HashSet * H, int hash_code){

    int indx=hash_code % H->locks_length;
    H->locks[indx] = 0;
}





//search value in bucket;
int list_search(struct node_t * Head,int val){
    
    struct node_t * curr;
    
    curr=Head;
    while(curr){
        if(curr->value==val) return 1;
        curr=curr->next;
    }
    return 0;
}


//add value in bucket;
//NOTE: duplicate values are allowed...
void list_add(struct HashSet * H, int key,int val,int hash_code){
    
    struct node_t * curr;
    struct node_t * next;
    struct node_t * node=(struct node_t *)malloc(sizeof(struct node_t));
    /*node->value=val;
    node->next=NULL;
    curr=H->table[key];
    if(curr==NULL){
        H->table[key]=node;
        return ;
    }
    while(curr->next){
        curr=curr->next;
        next=curr->next;
    }
    curr->next=node;
    */
    node->value=val;
    node->hash_code=hash_code;
    if(H->table[key]==NULL) node->next=NULL;
    else node->next=H->table[key];
    H->table[key]=node;
}


// delete from bucket. The fist value equal to val will be deleted
int list_delete(struct HashSet *H,int key,int val){
    
    struct node_t * curr;
    struct node_t * next;
    struct node_t * prev;

    curr=H->table[key];
    prev=curr;
    if((curr!=NULL)&&(curr->value==val)) {
        H->table[key]=curr->next;
        free(curr);
        return 1;
    }
    while(curr){
        if( curr->value==val){
            prev->next=curr->next;
            free(curr);
            return 1;
        }
        prev=curr;
        curr=curr->next;
    }
    return 0;
}





void initialize(struct HashSet * H, int capacity){
    
    int i;
    H->setSize=0;
    H->capacity=capacity;
    H->table = (struct node_t **)malloc(sizeof(struct node_t *)*capacity);
    for(i=0;i<capacity;i++){
        H->table[i]=NULL;
    }
    H->locks_length=capacity;
    H->locks=(int *)malloc(sizeof(int) * capacity);
    for(i=0;i<capacity;i++) H->locks[i]=0;

}


int policy(struct HashSet *H){
    return ((H->setSize/H->capacity) >4);
}

void resize(struct HashSet *);

int contains(struct HashSet *H,int hash_code, int val){
    
    lock_set(H,hash_code);
    int bucket_index = hash_code % H->capacity;
    int res=list_search(H->table[bucket_index],val);
    unlock_set(H,hash_code);
    return res;
}

//reentrant ==1 means we must not lock( we are calling from resize so we have already locked the data structure)
void add(struct HashSet *H,int hash_code, int val, int reentrant){
    
    if(!reentrant) lock_set(H,hash_code);
    int bucket_index = hash_code % H->capacity;
    list_add(H,bucket_index,val,hash_code);
    //H->setSize++;
    __sync_fetch_and_add(&(H->setSize),1);
    if(!reentrant) unlock_set(H,hash_code);
    if (policy(H)) resize(H);
}

int delete(struct HashSet *H,int hash_code, int val){
    
    lock_set(H,hash_code);
    int bucket_index =  hash_code % H->capacity;
    int res=list_delete(H,bucket_index,val);
    //H->setSize--;
    __sync_fetch_and_sub(&(H->setSize),1);
    unlock_set(H,hash_code);
    return res;
}


void resize(struct HashSet *H){
    
    int i;
    struct node_t * curr;
    int old_capacity = H->capacity;
    for(i=0;i<H->locks_length;i++) lock_set(H,i);
    if(old_capacity!=H->capacity) {
        for(i=0;i<H->locks_length;i++) unlock_set(H,i);
        return; //somebody beat us to it
    }
    int new_capacity =  old_capacity * 2;
    H->capacity =  new_capacity;

    struct node_t ** old_table = H->table;
    H->setSize=0;
    H->table = (struct node_t **)malloc(sizeof(struct node_t *)*new_capacity);
    for(i=0;i<new_capacity;i++){
        H->table[i]=NULL;
    }
    for(i=0;i<old_capacity;i++){
        
        curr=old_table[i];
        while(curr){
                int val = curr->value;
                int hash_code = curr->hash_code;
                //int bucket_index= hash_code % new_capacity;
                add(H,hash_code,val,1);
                curr=curr->next;
        }
    }
    free(old_table);
    for(i=0;i<H->locks_length;i++) unlock_set(H,i);
}

void print_set(struct HashSet * H){
    
    int i;
    for(i=0;i<H->capacity;i++){
        
        struct node_t * curr=H->table[i];
        while(curr){
            printf("(%d) ",curr->value);
            curr=curr->next;
        }
        printf("--\n");
    }
}

/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(int *array, size_t n)
{
    if (n > 1) 
    {
        size_t i;
        for (i = 0; i < n - 1; i++) 
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}
void main(int argc,char * argv[]){

    int num_threads=atoi(argv[1]);
    struct HashSet * H=(struct HashSet *) malloc(sizeof(struct HashSet));
    initialize(H,10);
    srand(time(NULL));
    int i,j;
    /*#pragma omp parallel for num_threads(8) shared(H) private(i,j)
    for(j=0;j<8;j++){
        for(i=0;i<100000;i++){
            add(H,i+j*100000,i+j*100000,0);
            //add(H,rand(),i,0);
        }
    }
    */
    /*
    timer_tt * timer;
    timer = timer_init(timer);
    timer_start(timer);
    int * op_table;
    int res,c;
    #pragma omp parallel for num_threads(num_threads) shared(H) private(c,j,res,op_table)
    for(i=0;i<num_threads;i++){
        op_table = (int *)malloc(sizeof(int)*(1000000/num_threads));
        for(j=0;j<(1000000/num_threads);j++) op_table[j]=rand()%10000;
        for(j=0;j<(1000000/num_threads);j++){
            add(H,op_table[j],op_table[j],0);
        }
    }
    timer_stop(timer);
    double result=timer_report_sec(timer);
    printf(" timer result %lf \n",result);
    */

    int finds=200000/num_threads;
    int deletes=0/num_threads;
    int inserts=800000/num_threads;
    timer_tt * timer;
    int * value_table;
    int * op_table;
    int count_finds;
    int count_deletes;
    int c,k,res;   
    #pragma omp parallel for num_threads(num_threads) shared(H) private(c,j,res,timer,k,value_table,op_table)
    for(i=0;i<num_threads;i++){
        value_table = (int *)malloc(sizeof(int)*(1000000/num_threads));
        op_table = (int *)malloc(sizeof(int)*(1000000/num_threads));
        for(j=0;j<(1000000/num_threads);j++) value_table[j]=rand()%100000;
        for(j=0;j<inserts;j++) op_table[j]=1;
        for(j=inserts;j<(inserts +finds);j++) op_table[j]=2;
        for(j=(inserts+finds);j<(inserts+finds+deletes);j++) op_table[j]=3;
        c=50;
        res=0;
        for(j=0;j<(1000000/num_threads);j++){
            if (op_table[j]==2) res++;
        }
        shuffle(op_table,1000000/num_threads);
        //printf(" res %d\n",res);
        __sync_synchronize();
        timer = timer_init(timer);
        timer_start(timer);
        for(j=0;j<(1000000/num_threads);j++){
            for(k=0;k<c;k++);
            if(op_table[j]==1) add(H,value_table[j],value_table[j],0);
            else if(op_table[j]==2) res=contains(H,value_table[j],value_table[j]);
            else res=delete(H,value_table[j],value_table[j]);
            
        }
        timer_stop(timer);
        printf("%thread %d timer %lf\n",omp_get_thread_num(),timer_report_sec(timer));
    }
    /*
    for(i=0;i<55;i++){
        add(H,i,i,0);
    }
    */
    //print_set(H);
    //printf("%d \n",H->setSize);
    return;

    
}
