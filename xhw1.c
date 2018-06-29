#include <asm/unistd.h>
#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>


#ifndef __NR_xdedup
#error xdedup system call not defined
#endif

struct sdata
{
        char  *infile1name;
        char  *infile2name;
        char  *outfilename;
        unsigned int Flagbits;
        int ofl;
};
int validate_files(struct sdata fp)
{
       if( access( fp.infile1name , F_OK ) == -1 )
       {
           printf("\n ERROR : %s File doesn't Exist \n",fp.infile1name);
           return -1;
       }
       if( access( fp.infile2name , F_OK ) == -1 )
       {
           printf("\n ERROR : %s File doesn't Exist \n",fp.infile2name);
           return -1;
       }
       
       if( access( fp.infile1name , R_OK ) == -1 ) 
       {
           printf("\n ERROR : %s file doesnot have Read permission \n",fp.infile1name);
           return -1; 
       }
       if( access( fp.infile2name , R_OK ) == -1 )
       {
           printf("\n ERROR : %s file doesnot have Read permission \n", fp.infile2name);
           return -1;
       }
       if( ( fp.Flagbits & 0x02) == 1 && access( fp.outfilename , F_OK ) == -1 )
       {
           if (access( fp.outfilename , W_OK ) == -1) 
           {
                printf("\n ERROR : %s file doesnot have write permission \n", fp.outfilename);
                return -1;
           }
       }
       return 0;

}
int main(int argc,char * const argv[])
{

        int rc;
        //void *dummy = (void *) argv[1];
        int cmdlnvar, nflag=0, pflag=0, debug=0;
        char *infile1;
        char *infile2 , *outfile ;
        unsigned int n = 0x01 , p = 0x02 , d = 0x04 ;
        unsigned int bitFlag = 0x0;
        struct sdata data_from_cmd;

        while( (cmdlnvar = getopt ( argc , argv , "npd" )  ) != -1)
        {
            switch( cmdlnvar )
            {
                case 'n': if (nflag==1)
                            printf("\nWARNING : -n is set multiple times ");
                          nflag=1;
                          bitFlag |= n;
                          break;
                case 'p': if (pflag==1)
                          printf("\nWARNING : -p is set multiple times ");
                          pflag=1;
                          bitFlag |= p;
                          break;
                case 'd': if (debug==1)
                            printf("\nWARNING : -d is set multiple times ");
                          debug=1;
                          bitFlag |= d;
                          break;
                case '?': printf("\nERROR : Invalid option");
                          break;
                default : abort();

            }
        }
    
        if (nflag == 1 && pflag == 1)
        {
                pflag = 0;
                bitFlag &= ~p;
        }

        if (nflag == 0 && pflag == 0 && debug == 0)
        {
                
                if (argc > 2 + optind )
                {
                    printf("\nERROR : Too many arguments ");
                    exit(0);
                }
                if ( argc < 2  + optind )
                {
                    printf("\nERROR : missing arguments passed");
                    exit(0);
                }
                infile1 = argv[ 0 + optind ];
                infile2 = argv[ 1 + optind ];
        }

        
        if (nflag==1)
        {
                if (argc > 2 + optind )
                    {
                    printf("\nERROR : Too many arguments ");
                    exit(0);
                    }
                if ( argc < 2  + optind )
                    {
                    printf("\nERROR : missing arguments passed");
                    exit(0);
                    }
                    infile1 = argv[ 0 + optind ];
                    infile2 = argv[ 1 + optind ];
                    //outfile = argv[ optind ];
       }
        if (pflag==1)
        {
                if (nflag != 1)
                {
                        if (argc > 3 + optind )
                            {
                            printf("\nERROR : Too many arguments ");
                            exit(0);
                            }
                            if (argc < 3 + optind )
                            {
                            printf("\nERROR : missing arguments passed");
                             exit(0);
                            }
                            infile1 = argv[ 0 + optind ];
                            infile2 = argv[ 1 + optind ];
                            outfile = argv[ 2 + optind ];
                }
        }
        if ( debug ==1)
        {
            if(nflag != 1 && pflag != 1)
            {    
                if (argc > 2 + optind )
                    {
                    printf("\nERROR : Too many arguments ");
                    exit(0);
                    }
                if ( argc < 2  + optind )
                    {
                    printf("\nERROR : missing arguments passed");
                    exit(0);
                    }
                    infile1 = argv[ 0 + optind ];
                    infile2 = argv[ 1 + optind ];
                    //outfile = argv[ optind ];
             
           }       
               printf("\n Debug Mode is On");
        }
        printf("\naction performed bit flags :%x\n infile1 = %s \n infile2 = %s\n outfile = %s\n",bitFlag,infile1,infile2,outfile);
        data_from_cmd.infile1name = infile1 ;
        data_from_cmd.infile2name = infile2 ;
        data_from_cmd.outfilename = outfile ;
        data_from_cmd.Flagbits = bitFlag ;
        if( access( data_from_cmd.outfilename , F_OK ) == -1 )
        {
               data_from_cmd.ofl=0; 
           
        }
        else 
               data_from_cmd.ofl=1;

        rc = validate_files(data_from_cmd);
        if ( rc < 0 )
                exit(rc);
        else
                rc = 0;
        rc = syscall(__NR_xdedup,(void *) &data_from_cmd);
        if (rc >= 0)
                printf("\nNumber of bytes common : %d \n ",rc);
        else 
                printf("\nERROR occured during dedup with errno : %d \n",errno);
        
        exit(rc);
}
           
