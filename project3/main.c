#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alloca.h>
#include <string.h>

#define ADDRESS_MASK  0xFFFF
#define OFFSET_MASK  0xFF
#define TLBS 16       /*TLB size*/
#define PTS 256  /*size of the page table*/
#define FS 256        /*size of the frame*/
#define TNOF 256  /*number of frames in all*/

int PTN[PTS];  /*holds page numbers*/
int PTF[PTS];   /*holds frame numbers*/

int pageFaults = 0;
int TLBHits = 0;
int FAF = 0;  /*keeps track of the frame that is first available to us*/
int FAPTN = 0;  /*tracks first avaialble table entry*/
int TLBcount = 0;

int TLBPageNumber[TLBS];
int TLBFrameNumber[TLBS];
int PM[TNOF][FS]; /*this is the array for physical memory*/

#define BUFFER_SIZE 10
#define BYTER 256

FILE    *addressesf;
FILE    *backing_store;
FILE    *out1;
FILE    *out2;
FILE    *out3;
/*array to store input addresses when opening the file*/
char address[BUFFER_SIZE];
int logical_address;
/*contains content from backing store bin*/
signed char buffer[BYTER];

signed char value;

void Pager(int address);
void BSreader(int PN);
void TLBFIFOPR(int PN, int FN);
/*takes logical address and gets physical address form it*/
void Pager(int logical_address){
    
    int PN = ((logical_address & ADDRESS_MASK)>>8);
    int offset = (logical_address & OFFSET_MASK);
    
    int FN = -1;
    
    int i;
    for(i = 0; i < TLBS; i++){
        if(TLBPageNumber[i] == PN){
            FN = TLBFrameNumber[i];
                TLBHits++;
        }
    }
    
    /*runs if correct frame number is found*/
    if(FN == -1){
        int i;
        for(i = 0; i < FAPTN; i++){
            if(PTN[i] == PN){
                FN = PTF[i];
            }
        }
        if(FN == -1){
            BSreader(PN);           /*file not found andpage fault, so we call to BSreader*/
            pageFaults++;
            FN = FAF - 1;
        }
    }
    
    TLBFIFOPR(PN, FN);  /*inserts page and frame number into TLB buffer*/
    value = PM[FN][offset];
             /*printf("frame number: %d\n", FN);
               printf("offset: %d\n", offset);*/
    /* for debugging use we print the addresses and value*/
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, (FN << 8) | offset, value);
    fprintf(out1, "%d\n", logical_address);
    fprintf(out2, "%d\n", (FN << 8) | offset);
    fprintf(out3, "%d\n", value);
}

/*Uses a FIFO based TLB update to fix room in the TLB for more data*/
void TLBFIFOPR(int PN, int FN){
    
    int i;
    for(i = 0; i < TLBcount; i++){
        if(TLBPageNumber[i] == PN){
            break;
        }
    }
    
    if(i == TLBcount){
        if(TLBcount < TLBS){
            TLBPageNumber[TLBcount] = PN;
            TLBFrameNumber[TLBcount] = FN;
        }
        else{
            for(i = 0; i < TLBS - 1; i++){
                TLBPageNumber[i] = TLBPageNumber[i + 1];
                TLBFrameNumber[i] = TLBFrameNumber[i + 1];
            }
            TLBPageNumber[TLBcount-1] = PN;
            TLBFrameNumber[TLBcount-1] = FN;
        }
    }
    
    /*if number of all the entries in not equal...*/
    else{
        for(i = i; i < TLBcount - 1; i++){
            TLBPageNumber[i] = TLBPageNumber[i + 1];
            TLBFrameNumber[i] = TLBFrameNumber[i + 1];
        }
        if(TLBcount < TLBS){
            TLBPageNumber[TLBcount] = PN;
            TLBFrameNumber[TLBcount] = FN;
        }
        else{
            TLBPageNumber[TLBcount-1] = PN;
            TLBFrameNumber[TLBcount-1] = FN;
        }
    }
    if(TLBcount < TLBS){                    /*updates the amount of pages in the TLB*/
        TLBcount++;
    }
}

/*function to read backing store bin and bring frame into the PM and the page table array */
void BSreader(int PN){
    /* seek to byte BYTER in the backing store bin */
    if (fseek(backing_store, PN * BYTER, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }
    
    /*read BYTER bytes from bin to buffer*/
    if (fread(buffer, sizeof(signed char), BYTER, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }
    
    /*placed in first avaialble frame*/
    int i;
    for(i = 0; i < BYTER; i++){
        PM[FAF][i] = buffer[i];
    }
    
    PTN[FAPTN] = PN;
    PTF[FAPTN] = FAF;
    
    /*update*/
    FAF++;
    FAPTN++;
}

/*calls pager and other functions to run the code properly, is the boss of the code*/
int main(int argc, char *argv[])
{
    /*in case of errors*/
    if (argc != 2) {
        fprintf(stderr,"error in the format of the command line\n");
        return -1;
    }
    
    backing_store = fopen("BACKING_STORE.bin", "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "error accessing BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }

    addressesf = fopen(argv[1], "r");
    if (addressesf == NULL) {
        fprintf(stderr, "error accessing addresses.txt %s\n",argv[1]);
        return -1;
    }
    out1 = fopen("out1.txt", "w");
    if (out1 == NULL) {
           fprintf(stderr, "error accessing out1.txt %s\n",argv[1]);
           return -1;
       }
    out2 = fopen("out2.txt", "w");
    if (out2 == NULL) {
           fprintf(stderr, "error accessing out2.txt %s\n",argv[1]);
           return -1;
       }
    out3 = fopen("out3.txt", "w");
    if (out3 == NULL) {
           fprintf(stderr, "error accessing out3.txt %s\n",argv[1]);
           return -1;
       }
    
    int NTA = 0;
    /*reads all logical addresses*/
    while ( fgets(address, BUFFER_SIZE, addressesf) != NULL) {
        logical_address = atoi(address);
        
        /*get PM*/
        Pager(logical_address);
        NTA++;  /*used for calculations later that require total addresses*/
    }
    
    /*statistics*/
    printf("Number of translated addresses = %d\n", NTA);
    double pfRate = pageFaults / (double)NTA;
    double TLBRate = TLBHits / (double)NTA;
    
    printf("Page Faults = %d\n", pageFaults);
    printf("Page Fault Rate = %.3f\n",pfRate);
    printf("TLB Hits = %d\n", TLBHits);
    printf("TLB Hit Rate = %.3f\n", TLBRate);
    
    /*free memory*/
    fclose(addressesf);
    fclose(backing_store);
    fclose(out1);
    fclose(out2);
    fclose(out3);
    
    return 0;
}
