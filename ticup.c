#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>


//Buffer size to read the event queue to
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))


//Inotify file descriptors, global to enable cleanup on ctrl-c
int inotify_d = -1;
int watch_d = -1;



//Makes a backup of the source file. Returns 0 if everything goes OK, 1 otherwise
int make_backup(char *src_path, char *backup_folder) {

  //Current time in microsecond resolution
  struct timeval current_time;
  gettimeofday(&current_time, NULL);
  struct tm *local_time = localtime(&current_time.tv_sec);

  //Extract the actual folename of the source file from the path
  char *src_filename = strrchr(src_path, '/');
  src_filename = src_filename != NULL ? src_filename + 1 : src_path;
  int src_filename_lenght = strlen(src_filename);

  //Buffer for the filename of the backup file
  int time_len = 4 + 2 + 2 + 1 + 2 + 2 + 2 + 6 + 6;
  int backup_filename_len = time_len + 1 + src_filename_lenght;
  char backup_filename[backup_filename_len + 2];

  //Print to the buffer
  sprintf(backup_filename, "%04d-%02d-%02d_%02d:%02d:%02d:%06d_%s",
          local_time->tm_year + 1900, local_time->tm_mon + 1,
          local_time->tm_mday, local_time->tm_hour,
          local_time->tm_min, local_time->tm_sec,
          current_time.tv_usec, src_filename);

  //Full path of the backup file
  char backup_path[strlen(backup_folder) + 1 + backup_filename_len + 1];
  strcpy(backup_path, backup_folder);
  strcat(backup_path, "/");
  strcat(backup_path, backup_filename);


  FILE *ifd = fopen(src_path, "r");
  if (ifd == NULL) {
    return 1;
  }

  FILE *ofd = fopen(backup_path, "w");
  if (ofd == NULL) {
    return 1;
  }

  //Copy the file
  char c = fgetc(ifd);
  while (c != EOF) {
    fputc(c, ofd);
    c = fgetc(ifd);
  }

  fclose(ifd);
  fclose(ofd);
}




//Checks that the arguments are there and point to existing places. Returns 0 if everything is ok, 1 otherwise
int validate_arguments(int argc, char **argv){

  //Number of arguments
   if (argc != 3) {
    printf("Usage: ticup [path to file being watched] [path to the backup "
           "folder]\n");
    return 1;
  }
  

  //If backup folder has a trailing /, remove it
  int bdir_len = strlen(argv[2]);
  if(argv[2][bdir_len - 1] == '/'){
    argv[2][bdir_len - 1] = 0;
  }

  //Get arguments' stats(and check they exist in the process)
  struct stat bdir_stat, src_stat;

  if(stat(argv[1], &src_stat) != 0){
    perror("Can't access source file");
    return 1;
  }

  if(stat(argv[2], &bdir_stat) != 0){
    perror("Can't access backup directory");
    return 1;
  }


  //Check that argument 1 is not a dir and argument 2 is a dir
  if(S_ISDIR(src_stat.st_mode)){
    fprintf(stderr, "Source file seems to be a directory\n");
    return 1;
  }
  
  if(!S_ISDIR(bdir_stat.st_mode)){
    fprintf(stderr, "Backup directory does not seem to be a directory\n");
    return 1;
  }

  //Check that the program has the necessary permissions
  if(access(argv[1], R_OK) != 0) {
    perror("Can't read file being watched");
    return 1;
  }

  if(access(argv[2], W_OK) != 0){
    perror("Can't write to backup directory");
    return 1;
  }

  return 0;
}


//Cleans up inotify handles, if they are valid
void cleanup(){
  if(watch_d >= 0) inotify_rm_watch(inotify_d, watch_d);
  if(inotify_d >= 0) close(inotify_d);
}

//Handler to clean up even on ctrl-c
void sigint_handler(int sig_num){
  cleanup();
  exit(0);
}




int main(int argc, char **argv) {


  //Validate arguments and set up ctrl-c handler
  if(validate_arguments(argc, argv) != 0){
    return 1;
  } 

  signal(SIGINT, sigint_handler);


  //Make initial backup
  make_backup(argv[1], argv[2]);

  //Init inotify system
  inotify_d = inotify_init();

  if (inotify_d < 0) {
    perror("inotify_init failed!");
    return 1;
  }

  watch_d = inotify_add_watch(inotify_d, argv[1], IN_CLOSE_WRITE | IN_MOVE_SELF);


  //Buffer to read the events to
  char buffer[BUF_LEN];


  int should_stop = 0;
  while (!should_stop) {

    //Read the events(blocks until there are some)
    int length = read(inotify_d, buffer, BUF_LEN);

    if (length < 0) {
      perror("failed reading inotify watcher");
      return 1;
    }

    //Loop over events
    int i = 0;
    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      printf("%#010x\n", event->mask);

      //Stop if the file is moved or the watcher is removed (happens if file is deleted, for example)
      if (event->mask & (IN_IGNORED | IN_MOVE_SELF)) {
        should_stop = 1;

      //Make backup if file is closed in writing mode
      } else if (event->mask & IN_CLOSE_WRITE) {
        make_backup(argv[1], argv[2]);
      }

      i += EVENT_SIZE + event->len;
    }
  }

  cleanup();

  exit(0);
}