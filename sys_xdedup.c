#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/namei.h>


struct sdata
{
        char  *infile1name;
        char  *infile2name;
        char  *outfilename;
        unsigned int Flagbits;
        int ofl;
};

int length_string(char *str, bool d)
{
        int i = 0;
        while(str[i]!='\0')
        {
                i++;
        }
        return i;
}

void string_cat(char *dest, char *src, char * result)
{
    int i = length_string(dest,0) ,j = length_string(src,0);
    char res[(i+j)];
    for (i = 0; dest[i] != '\0'; i++)
    {
        res[i]=dest[i];
    }
    for (j = 0; src[j] != '\0'; j++)
        res[i+j] = src[j];
    res[i+j] = '\0';
    memcpy(result,res,(i+j));
    return ;
}

asmlinkage extern long (*sysptr)(void *arg);

struct file * open_file(char * const filepath, int acces, int offset, bool d , int ferr)
{ 
         struct file *flip=NULL;
         mm_segment_t old_fs;
         int errno=0;
         if(d)
         {
               printk("\n************** openning file : %s  *******************", filepath);
         }

         old_fs = get_fs();
         set_fs(get_ds());
         flip = filp_open(filepath, acces, offset);
         set_fs(old_fs);
         if (IS_ERR(flip))
         {
                 errno = PTR_ERR(flip);
                 ferr=errno;
                 return NULL;
         }

         if(d)
         {
               printk("\n************** %s file opened succesfully  *******************\n",filepath);
         }

         return flip;

}

void close_file(struct file *flip,bool d)
{
        filp_close(flip, NULL);
        if(d)
        {
              printk("\n************** file closed succesfully  *******************");
        }

        return ;
}

int read_file(struct file *flip, unsigned  int len,unsigned char *data, unsigned long long  offset,bool d)//,void *buffer
{
        mm_segment_t old_fs;
        int return_data;       

        /*if(!flip->f_op->read)
        {
                printk("file doesn't allow  to read");
                return -2;
        }*/
        old_fs=get_fs();
        set_fs(get_ds());
        return_data=vfs_read(flip,data,len,&flip->f_pos);
        set_fs(old_fs);;
        if(d)
        {
              printk("\n************** %s file read successfully *******************\n data read : %s\n\n ",flip->f_path.dentry->d_iname,data);
        }

        return return_data;

}

int write_file(struct file *flip, unsigned  int len,unsigned char *data, unsigned long long  offset,bool d)
{
        mm_segment_t old_fs;
        int return_data = -1;
        flip->f_pos = 0;
        old_fs = get_fs();
        set_fs(get_ds());
        return_data = vfs_write( flip , data , len , &offset ) ;
        set_fs(old_fs);        
        if(d)
        {
              printk("\n************** %s file was written successfully *******************\n data wirtten : %s\n\n ",flip->f_path.dentry->d_iname,data);
        }

        return return_data;
}

int  warpfs_read(char  *infile1name, char  *infile2name, char  *outfilename, bool n, bool p, bool d)
{
        int f1=0,f2=0,f3=0;
        struct file *infile1=open_file(infile1name,O_RDONLY,0,d,f1);
        struct file *infile2=open_file(infile2name,O_RDONLY,0,d,f2);
        struct file *outfile=NULL;
        struct inode *inode1 = infile1->f_inode;
        struct inode *inode2 = infile2->f_inode;
        int file1_len=( inode1->i_size);
        int file2_len=( inode2->i_size);
        int page_size = PAGE_SIZE;
        int i = 0 , j = 0 , file1_buf_int = 0 , file2_buf_int = 0 , outfile_buf_int = 0 ;
        int deduped_bytes = 0;
        int num_of_pages = file1_len/page_size;
        char *temp_data = kmalloc ( sizeof(char) * page_size, GFP_KERNEL);
        char *file1_data = kmalloc( page_size, GFP_KERNEL);
        char *file2_data = kmalloc( page_size, GFP_KERNEL);
        
        if (f1 == -ENOENT)
             return -EACCES; 

        if (f2 == -ENOENT)
             return -EACCES;

        if (p)
        {
               outfile=open_file( outfilename , O_WRONLY | O_CREAT , 0 , d ,f3);
               
        }
        if (d)
        {
                printk("\n\n\n************** in warpfs_read method *******************\n\n\n ");
        }
        if (file1_len> num_of_pages*page_size)
        {
                num_of_pages+=1;
        } 
        printk("\n size 1 : %d\n size 2 : %d \n",file1_len,file2_len);
        if (file1_len!=file2_len)
        {
                if (n && ~ p)
                {
                        if (d)
                        {
                                printk("\n************** ERROR : two files are not identical. CAiNNOT DEDUP ******************* ");
                        } 
                        return -EPERM;
                }
                if ( n==0 && p==0)
                {
                        return -EPERM;
                }                
        }	
        infile1->f_pos = 0;
        infile2->f_pos = 0;
	for( i = 0 ; i < num_of_pages ; i++ )
        { 
                file1_buf_int = read_file( infile1, page_size, file1_data, i*page_size, d);
                if (file1_buf_int < 0 )
                {
                     return file1_buf_int;
                }
                file2_buf_int = read_file( infile2, page_size, file2_data, i*page_size, d);
                if (file2_buf_int < 0 )
                {
                     return file2_buf_int;
                }
                if (strcmp(file1_data,file2_data) == 1)
                {
                        j=0;
                        while ( ( file1_data[j] == file2_data[j] ) )
                        {
                                temp_data[j] = file1_data[j];
                                j++;
                        }
                        deduped_bytes+=j;
                        if (p)
                        {
                                outfile_buf_int = write_file( outfile , j , temp_data, i*page_size, d);
                                if ( outfile_buf_int < 0 )
                                {
                                        return outfile_buf_int;
                                } 
                        }                        
                }
                else
                {
                                deduped_bytes+=file1_buf_int;
                                if (d)
                                {
                                        printk("\n deduped_bytes : %d \n",deduped_bytes);
                                }
                                if (p)
                                {
                                        outfile_buf_int = write_file( outfile ,file1_buf_int, file1_data, i*page_size, d);
                                        if ( outfile_buf_int < 0 )
                                        {
                                                 return outfile_buf_int;
                                        }
                                }
                }
                if (d)
                { 
                          printk("\n bytes : %d \n",deduped_bytes);         
                }
        }
        close_file(infile1,d);
        close_file(infile2,d);
        kfree(temp_data);
        kfree(file1_data);
        kfree(file2_data);
        if (p)
        {
                close_file(outfile,d);
        }
        
        return deduped_bytes;
}

static int link_files(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry, bool d)
{
        int err =  -1;
        if (d)
        {
                printk("\n************** in link_files method ******************* ");
        }
        err = vfs_link( old_dentry, dir , new_dentry, NULL);
        if (d)
        {
                printk("\n************** end of link_files method ******************* ");
        }
        return 0;
}
static int unlink_files(struct inode *parent_dir, struct dentry *victim_dentry, bool d)
{
        int err = -1;
        if (d)
        {
                printk("\n************** in unlink_files method ******************* ");
        }
        err = vfs_unlink( parent_dir , victim_dentry , NULL);
        if (d)
        {
                printk("\n************** end of unlink_files method ******************* ");
        }
        return 0;
}
void lock_file(struct inode *lock_inode, bool d)
{
        
        if (d)
        {
                printk("\n************** in lock_file method ******************* ");
        }
        inode_lock(lock_inode);
        if (d)
        {
                printk("\n************** end of lock_file method ******************* ");
        }
        return;
}
void unlock_file(struct inode *unlock_inode, bool d)
{
        
        if (d)
        {
                printk("\n************** in unlock_file method ******************* ");
        }
        inode_unlock(unlock_inode);
        if (d)
        {
                printk("\n************** end of unlock_file method ******************* ");
        }
        return;
}
struct dentry * lookup_a_file(const char *name, struct dentry *base, int len, bool d)
{
        struct dentry *lookedup_dentry;
        if (d)
        {
                printk("\n************** in lookup_a_file method ******************* ");
        }
        lookedup_dentry=lookup_one_len(name, base, len);
        if (d)
        {
                printk("\n************** end of lookup_a_file method ******************* ");
        }
        return lookedup_dentry;
}

int rename_file(struct inode *old_parent_inode, struct dentry *old_dentry, struct inode *new_parent_inode, struct dentry *new_dentry, unsigned int flags, bool d)
{
        int err = -1;
        if (d)
        {
                printk("\n************** in start of rename_file method *******************\n ");
        }
        err = vfs_rename( old_parent_inode ,old_dentry, new_parent_inode , new_dentry, NULL, flags);
        if (d)
        {
                printk("\n************** %s is successfully renamed as %s *******************\n ",old_dentry->d_iname,new_dentry->d_iname);
        }
        return 0;

}

char * str_add( char *src, char *dest, bool d)
{
    int i = 0, j = 0;
	
	for(i = 0; src[i] != '\0' ; i++)
	{
	}
	for(j = 0; dest[j] != '\0' ; j++)
	{
	    src[i+j] = dest[j] ;
	}
	src[i+j] = '\0';
	if (d)
	{
	   printk("\n********************* str concated : %s ************************",src);
	}
	return src;
}
struct file * creating_file( char * filename, int flags , bool d )
{
        struct file * new_file = NULL;

        if (d)
        {
           printk("\n********************* in creating_file ************************");
        }
	new_file = filp_open(filename,O_WRONLY| O_CREAT , 0);       
        if (d)
        {
           printk("\n********************* %s file created successfully  ************************",filename);
        }
        return new_file;
}

int wrapfs_rename(char * source_name, char * destination_name, bool d)
{
        int f1=0;
        struct file *source_file = open_file( source_name, O_RDONLY, 0, d, f1);
        struct dentry *source_dentry = source_file -> f_path.dentry;
        struct dentry *lookedup_destination_dentry = NULL;
        struct dentry *source_parent_dentry = source_file -> f_path.dentry -> d_parent;
        struct dentry *lookedup_destination_parent_dentry = NULL ;
        struct inode *source_parent_inode = source_parent_dentry -> d_inode;
        struct inode *lookedup_destination_parent_inode = NULL ; 
        int destination_name_len = length_string( destination_name , d );
        u_int destination_flags = source_file -> f_flags ;
        int errno = -1;

        if (d)
        {
                printk("\n************** in wrapfs_rename method ******************* ");
        }
          
               
 
        lock_file( source_parent_inode , d );
        lookedup_destination_dentry = lookup_a_file( destination_name, source_parent_dentry, destination_name_len, d); 
        unlock_file(source_parent_inode,d);
     
        if (d)
        {
                printk("\n************** lookedup_destination_dentry done successfully for file :%s ******************* ",destination_name);
        }

        lookedup_destination_parent_dentry = lookedup_destination_dentry -> d_parent;
        lookedup_destination_parent_inode = lookedup_destination_parent_dentry -> d_inode;       
        close_file(source_file,d);       

        lock_rename( lookedup_destination_parent_dentry, source_parent_dentry);
        errno = rename_file( source_parent_inode , source_dentry , lookedup_destination_parent_inode , lookedup_destination_dentry , destination_flags, d );
        unlock_rename( lookedup_destination_parent_dentry, source_parent_dentry);
        if (d)
        {
                printk("\n************** end of wrapfs_rename method ******************* ");
        }
        return 0;
}

static int wrapfs_link(char * old_file_name, char * new_file_name, bool d)
{
        int f1=0, f2=0  ,ft=0;
        struct file *old_file = open_file( old_file_name, O_RDONLY, 0, d,f1);
        struct file *new_file = open_file( new_file_name, O_RDONLY, 0, d,f2);
        struct file *new_file_temp;
        struct dentry *old_dentry = old_file -> f_path.dentry;
        struct dentry *new_dentry = new_file -> f_path.dentry;
        struct inode *old_dir_parent = old_dentry -> d_parent -> d_inode;
        struct inode *new_dir_parent = new_dentry -> d_parent -> d_inode;
        int errno_link;//, errno_unlink, errno_lock, errno_unlock, error_lookup;
        struct dentry *lookedup_dentry;
        char * new_temp_name = ( char * ) old_dentry -> d_iname, *t ; 
        t = kmalloc(100,GFP_KERNEL);
        string_cat(new_temp_name,new_file_name  ,t);

        if (d)
        {
                printk("\n************** in wrapfs_link method ******************* ");
        }

	if ( old_dentry -> d_inode -> i_ino  == new_dentry -> d_inode -> i_ino ) 
        {
             return -2;
        }
        
        new_file_temp  = open_file( t , O_CREAT, 0, d,ft);
        close_file(new_file_temp,d);
        close_file(old_file,d);
        errno_link = wrapfs_rename( old_file_name, t  , d );

        if ( errno_link < 0 )
        { 
              if (d)
                   printk("\n ERROR : failure while hard liking   \n");

              new_file_temp  = open_file( t  , O_RDONLY, 0, d,ft);
              lock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
              errno_link=unlink_files(new_file_temp -> f_path.dentry -> d_parent -> d_inode , new_file_temp -> f_path.dentry ,d);
              unlock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
   
              if ( errno_link < 0 )
              {    
                   close_file( new_file_temp ,d );
                   close_file( new_file , d );
                   kfree(t);
                   if (d)
                        printk("\n ERROR : Unlink of temp file failed \n");
                   return errno_link ;
              }
              close_file( new_file_temp ,d );
              close_file( new_file , d );
              kfree(t);
              return errno_link;
  
        }
       /* lock_file(old_dir_parent,d);
        errno_link=unlink_files(old_dir_parent,old_dentry,d);
        unlock_file(old_dir_parent,d);
        if ( errno_link < 0 )
        { 
               close_file(new_file,d); 
               return -82;
        }

      */ close_file(new_file,d);
        lock_file(old_dir_parent,d);
        lookedup_dentry=lookup_a_file(old_file_name,old_dentry->d_parent,length_string(old_file_name,d),d);
        unlock_file(old_dir_parent,d);

        if(d)
        {
               printk("\n ********************* successful lookup *********************\n");
        }

        lock_file(new_dir_parent,d);
        errno_link=link_files(new_dentry,new_dir_parent,lookedup_dentry,d);
        unlock_file(new_dir_parent,d); 
       
        if (errno_link < 0)
        {
              if (d)
                        printk("\n ERROR : Failure during linking the files  \n");

              new_file_temp  = open_file( t , O_RDONLY, 0, d,ft);
              lock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
              errno_link=unlink_files(new_file_temp -> f_path.dentry -> d_parent -> d_inode , new_file_temp -> f_path.dentry ,d);
              unlock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
              if ( errno_link < 0 )
              {
                   close_file( new_file_temp ,d );
     
                   kfree(t);
                   if (d)
                        printk("\n ERROR : Failure during unlinking the old file  \n");

                   return errno_link ;
              }
              close_file( new_file_temp ,d );
     
              kfree(t);
              return errno_link;

        }
        if (errno_link>=0)
        { 
              new_file_temp  = open_file( t , O_RDONLY, 0, d,ft);
              lock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
              errno_link=unlink_files(new_file_temp -> f_path.dentry -> d_parent -> d_inode , new_file_temp -> f_path.dentry ,d);
              unlock_file(new_file_temp -> f_path.dentry -> d_parent -> d_inode ,d);
              close_file( new_file_temp , d );
            
        }
        if (d)
        {
                printk("\n************** end of wrapfs_link method ******************* ");
        }
      
        kfree(t);
        return errno_link;
}

int get_common_file_bytes(char  *infile1name, char  *infile2name , bool d)
{
    int f1=0,f2=0;
    struct file *infile1=open_file(infile1name,O_RDONLY,0,d,f1);
    struct file *infile2=open_file(infile2name,O_RDONLY,0,d,f2);
    int page_size = PAGE_SIZE;
    int file1_len=( infile1->f_inode->i_size);
    int file2_len=( infile2->f_inode->i_size);
    int i = 0 , j = 0 , file1_buf_int = 0 , file2_buf_int = 0 ;
    int deduped_bytes = 0;
    int num_of_pages = file1_len/page_size;
    char *file1_data = kmalloc( page_size, GFP_KERNEL);
    char *file2_data = kmalloc( page_size, GFP_KERNEL);
    if (d)
    {
            printk("\n\n\n************** in get_common_file_bytes method *******************\n\n\n ");
    }
    if (file1_len> num_of_pages*page_size)
    {
            num_of_pages+=1;
    }
    if (file1_len!=file2_len)
    { 
            if (d)
                        printk("\n ERROR : Files length not identical   \n");

            return -EPERM;
    }
    
    infile1 -> f_pos = 0;
    infile2 -> f_pos = 0;
    for (i = 0 ; i < num_of_pages ; i++ )
    { 
            file1_buf_int = read_file( infile1, page_size , file1_data, i*page_size, d);
            if (file1_buf_int < 0 )
            {
                   if (d)
                        printk("\n ERROR : Failed to open file in READONLY mode   \n");

                   return file1_buf_int;
            }
            file2_buf_int = read_file( infile2, page_size, file2_data, i*page_size, d);
            if (file2_buf_int < 0 )
            {
                   if (d)
                        printk("\n ERROR : Failed to open file in READONLY mode  \n");

                   return file2_buf_int;
            }
         
            if (strcmp(file1_data,file2_data) == 1)
            { 
                    j=0;
                    while ( ( file1_data[j] == file2_data[j] ) )
                    {
                            j++;
                    }
                    deduped_bytes+=j;
            }
            else
            {
                    deduped_bytes += file1_buf_int;
            }         
    }
    close_file(infile1,d);
    close_file(infile2,d);
    kfree(file1_data);
    kfree(file2_data);
    if (d)
    {
            printk("\n bytes : %d \n",deduped_bytes);
    }
    return deduped_bytes;
}

int validation_files(char *infile1name, char *infile2name, char *outfilename, bool n , bool p , bool d )       
{
        int f1=0, f2=0, f3=0;
        struct file *infile1 = open_file(infile1name,O_RDONLY,0,d,f1);
        struct file *infile2 = open_file(infile2name,O_RDONLY,0,d,f2);
        struct file *outfile = NULL;
        struct inode *inode1 = infile1 -> f_inode;
        struct inode *inode2 = infile2 -> f_inode;
        struct inode *inode3 ;
        
        
        if ( f1 == -ENOENT)
              return -EINVAL;
        if ( f2 == -ENOENT)
              return -EINVAL;

        if ( n == 0 && p==0 )
        {
              if (inode1 -> i_ino  == inode2 -> i_ino)
              { 
                  if (d)
                        printk("\n ERROR : Both input files are referencing the same inode  \n");

                  return -EPERM;
              }
        }
        if (p)
        {
              outfile = open_file(outfilename,O_WRONLY,0,d,f3);
              if (f3 == -ENOENT)
                       goto label1 ;          
              inode3 = outfile -> f_inode;
        }
        if ( p == 1 && inode1 -> i_ino == inode3 -> i_ino) 
        {
              if (d)
                        printk("\n ERROR : output file is a SYMLINK of a input file   \n");

              return -EPERM;
        }
        if ( p == 1 && inode2 -> i_ino == inode3 -> i_ino) 
        {
              if (d)
                        printk("\n ERROR : output file is a SYMLINK of a input file  \n");
              return -EPERM;
        }
        label1 :
        if ( n == 0 &&  inode1 -> i_ino  == inode2 -> i_ino)
        { 
              if (d)
                        printk("\n ERROR : CANNOT perform dedup as both files are referencing the same inode  \n");

              return -EPERM;
        }
        if ( p==0 && ( int) inode1 -> i_size != (int) inode2 -> i_size)
        { 
              if (d)
                        printk("\n ERROR : File sizes are not Identical  \n");
              return -EPERM;
        }
        close_file(infile1,d);
        close_file(infile2,d);
        if ( p == 1 )
        {
                  close_file(outfile,d);
        }
        return 0;

}
asmlinkage long xdedup(void *arg)
{
        char *data ,*temp_name, *t ;
	struct sdata *data_from_cmd=arg;
        struct file *tempout=NULL;
        unsigned int n = 0x01 , p = 0x02 , d = 0x04 ;
        bool nflag =( n & data_from_cmd->Flagbits) , pflag = (p & data_from_cmd->Flagbits) , debug = (d & data_from_cmd->Flagbits) ;
        int tem=0, tem1, errno;

        data=kmalloc(PAGE_SIZE,GFP_KERNEL);
    
        if ( pflag== 1 && data_from_cmd->ofl==0)
        {
 
            tempout = creating_file( data_from_cmd->outfilename ,0 , debug );
            close_file(tempout,debug);
 
        }    
        tem = validation_files(data_from_cmd->infile1name, data_from_cmd->infile2name, data_from_cmd->outfilename, nflag , pflag, debug);
        if (tem != 0)
        {
            return tem;
        }  
     
        if ( nflag == 1  && pflag == 0 ) 
        {
             tem= warpfs_read(data_from_cmd->infile1name, data_from_cmd->infile2name, data_from_cmd->outfilename, nflag, pflag, debug);        
        }

        if ( pflag == 1 )
        { 
                 t = kmalloc(100,GFP_KERNEL);
                 temp_name = "output_temp_v1_"; 
                 string_cat(temp_name,(char *) data_from_cmd->outfilename ,t);
                 
                 tempout = creating_file( t ,0 , debug );
                 close_file(tempout,debug);
                 tem= warpfs_read(data_from_cmd->infile1name, data_from_cmd->infile2name, t  , nflag, pflag, debug);
                 tempout = open_file( t ,O_WRONLY ,0,debug,0);
                 errno = wrapfs_rename( t , data_from_cmd->outfilename , debug ); 
                 close_file(tempout,debug);
                 kfree(t);
                 
        }
        if (pflag == 0 && nflag == 0)// && tem >= 0)
        {          
                  
                  tem = get_common_file_bytes (data_from_cmd->infile1name, data_from_cmd->infile2name,debug); 
                  tempout = open_file(data_from_cmd->infile1name    ,O_WRONLY ,0,debug,0);  
                  errno = tempout -> f_inode -> i_size;              
                        
                  if (tem  == errno) 
                  {
                         tem1=wrapfs_link( data_from_cmd->infile1name, data_from_cmd->infile2name,  debug);
                         if (tem1 == -2)
                         { 
                              return -EINVAL;
                         }
                  }
             
                  //printk("\n ******************** d  final result 2: %d  %d  **************\n",tem,errno);
        }
        kfree(data);
        return tem;
        if (debug)
        {
                printk("\n ********** nflag : %x \n ********** pflag : %x\n ********** dflag : %x\n",nflag,pflag,debug);
        }
        if (arg == NULL)
                return -EINVAL;
        else
                return 0;
//        return tem;
}

static int __init init_sys_xdedup(void)
{
        printk("installed new sys_xdedup module\n");
        if (sysptr == NULL)
                sysptr = xdedup;
        return 0;
}
static void  __exit exit_sys_xdedup(void)
{
        if (sysptr != NULL)
                sysptr = NULL;
        printk("removed sys_xdedup module\n");
}
module_init(init_sys_xdedup);
module_exit(exit_sys_xdedup);
MODULE_LICENSE("GPL");

                                                                                                    
