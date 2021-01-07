#include <bits/stdc++.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <cstdio> 
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <iomanip>

using namespace std;

void printdir(int l, int r, vector<struct f> &files);
void editorRefreshScreen();
void initCommandmode();
vector<string> get_tokens(string command);
bool isDirectory(string path);
void execute_command();
void create_file(vector<string> &tokens);
void create_dir(vector<string> &tokens);
void copy_file(string from, string to);
void copy_dir(string from, string to);
int copy_rec(vector<string> &tokens);
void delete_file(vector<string> &tokens);
void delete_dir(vector<string> &tokens);
void delete_rec(string d_path);
void search(vector<string> &tokens);
bool search_rec(string search_in, string name);
string process_path_string(string path);
void go_to(vector<string> &tokens);
void move(vector<string> &tokens);
void rename(vector<string> &tokens);


#define CTRL_KEY(k) ((k) & 0x1f)

// This will store all the various attributes of file/directory
struct f{
    string name;
    string size;
    string path;
    string type;
    string uid;
    string gid;
    string permissions;
    string lmodified;
};

// Will store editor configuration
struct editorConfig {
    struct termios orig_termios;
    int count;
    int offset = 10;                //gap b/w  bottom of scree and last file displayed

    int cx = 1;
    int cy = 1;                     //Initial position of cursor

    int screenrows;                  //length of terminal window
    int screencols;                 //widht of terminal window

    stack<string> forward_stack;     //stores next visited directory
    stack<string> back_stack;       //stores previoous visited directory

    int l;                          // pointer to vector storing attributes of files
    int r;                         // pointer to vector storing attributes of files
    int jump;                      //No. of files to be displayed on basis onf offset and terminal size
    vector<struct f> files;        //vector storing attributes of files
    int files_index = 0;           //points to a file attr. in vector corresponding to cursor in screen
    string mode;                   //stores the Normal or Command mode

    string root = "/home/jarves/NewFolder/IIITH Classes/Advanced Operating Systems/Assignments";
    string cur_dir = "";

    string command_string = "";
};

struct editorConfig E;

// returns these numbers corresponding to keys pressed
enum editorKey{
  ARROW_UP  = 1000,
  ARROW_DOWN = 1001,
  key_l = 1002,
  key_r = 1003,
  Enter = 1004,
  colon = 1005,
  ARROW_RIGHT = 1006,
  ARROW_LEFT = 1007,
  key_h = 1008,
};

string getAbsPath(string path){
  string result =  E.cur_dir + "/" + path;
  return result;
}

// gets triggered when terminal is resized
void winsz_handler(int sig) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); //get size of window
    E.screenrows = w.ws_row;
    E.jump = min(E.screenrows- E.offset,int(E.files.size())-1);
    
    E.r = E.l+E.jump -1;
   
    printdir(E.l,E.r,E.files); //Displays the files depending upon values of l and r
}

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

//get out of Non-canonical mode
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

// Refresh the screen
void editorRefreshScreen() {
  printf("\033[H\033[J"); //clear the screen
  E.cy = 1;
}

//Enable Non-Canonical mode
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 4;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void printdir(int l, int r, vector<struct f> &files){
    editorRefreshScreen();
    int name_width = 34;
    int size_width = 7;
    for(int i = l; i<=r; i++){
        cout << setw(name_width) << left << setfill(' ') << files[i].name << "\t" << setw(size_width) << right 
        << setfill(' ') << files[i].size << "\t" << setw(3) << right << setfill(' ') << files[i].type << "\t" 
        << files[i].uid << "\t" << files[i].gid << "\t"<< files[i].permissions << "\t" << files[i].lmodified << "\r\n";
    }
    if(E.mode == "Normal"){
      cout << "\r\n" << "\r\n";
      cout << ":Normal Mode\r\n";
    }

    write(STDOUT_FILENO,"\x1b[H", 3); // Move the cursor to top
}

string getPermission(const struct stat fileStat){
    
    string permission;
    permission += (fileStat.st_mode & S_IRUSR) ? "r" : "-";
    permission += (fileStat.st_mode & S_IWUSR) ? "w" : "-";
    permission += (fileStat.st_mode & S_IXUSR) ? "x" : "-";
    permission += (fileStat.st_mode & S_IRGRP) ? "r" : "-";
    permission += (fileStat.st_mode & S_IWGRP) ? "w" : "-";
    permission += (fileStat.st_mode & S_IXGRP) ? "x" : "-";
    permission += (fileStat.st_mode & S_IROTH) ? "r" : "-";
    permission += (fileStat.st_mode & S_IWOTH) ? "w" : "-";
    permission += (fileStat.st_mode & S_IXOTH) ? "x" : "-";
    return permission;
}

int getallfiles(string s, vector<struct f> &files){
  editorRefreshScreen();
    
  struct dirent *dirent_ptr;      // Pointer for directory entry
    
  int count = 0;  //count the number of directories and files present inside the directory
  DIR *dr = opendir(s.c_str()); // opendir() returns a pointer of DIR type.

  if (dr == NULL){  // opendir returns NULL if couldn't open directory
    cout << "Could not open current directory";
    return -1;
  }
  
  files.clear();                               //clear the vector of files before inserting new ones
  while ((dirent_ptr = readdir(dr)) != NULL){  //Traverse the current direct and store the file attr. present in a vector
            struct stat stat_ptr;
            struct f file_info;
            
            count++; // Count the no. of files present in the dir
            string abs_path_of_file = s + "/" + dirent_ptr->d_name;
            stat(abs_path_of_file.c_str(), &stat_ptr);
            
            file_info.name = dirent_ptr->d_name;
            file_info.size = to_string(stat_ptr.st_size);
            file_info.path =  abs_path_of_file;
            file_info.type = (S_ISDIR(stat_ptr.st_mode)) ? "d" : "-";
            file_info.uid = (!getpwuid(stat_ptr.st_uid))?"":getpwuid(stat_ptr.st_uid)->pw_name;
            file_info.gid = (!getgrgid(stat_ptr.st_gid))?"":getgrgid(stat_ptr.st_gid)->gr_name;
            file_info.permissions = getPermission(stat_ptr);
            file_info.lmodified = ctime(&stat_ptr.st_mtime);
            file_info.lmodified = file_info.lmodified.substr(0, file_info.lmodified.size() - 1);

            files.push_back(file_info);
    }
    
    E.cur_dir = s; //store the curr_dir 
    E.l = 0;
    E.jump = min(E.screenrows- E.offset,int(files.size())-1);
    E.r = E.l + E.jump -1;

    E.files_index = 0;

    printdir(E.l,E.r,files);
    closedir(dr);
    return count;
}

//This function detects the key press, processes it and returns the value
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    
    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }
    return Enter;
  }
  else if(c== 'l' and E.mode == "Normal") {
      return key_l;
  }
  else if(c== 'r' and E.mode == "Normal"){
      return key_r;
  }
  else if(c=='h' and E.mode == "Normal"){
    return key_h;
  }
  else if(c == '\r'){
      return Enter;   
  }
  
  else{
      return c;
  }
}

//get size of terminal
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

// Moves the cursor according to keys pressed
void editorMoveCursor(int key){
  switch(key) {
    // case ARROW_LEFT:
    //   E.cx--;
    //   break;
    // case ARROW_RIGHT:
    //   E.cx++;
    //   break;
    case ARROW_UP:
      E.cy--;
      E.files_index--;
      write(STDOUT_FILENO,"\x1b[1A",4);
      break;
    
    case ARROW_DOWN:
      write(STDOUT_FILENO,"\x1b[1B",4);
      E.cy++;
      E.files_index++;
      break;
  }
}

// calls various functions based on keys pressed and mode of the editor
void editorProcessKeypress() {
  int c = editorReadKey();
  
  if(E.mode == "Normal"){
    switch (c){
      case CTRL_KEY('q'):
        editorRefreshScreen();
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

      case ARROW_UP:
        if (E.mode == "Normal" && E.cy > 1){ //&& E.cy > 1
          editorMoveCursor(c);
        }
        break;

      case ARROW_DOWN:
        if (E.mode == "Normal" && E.cy < E.jump){
          editorMoveCursor(c);
        }
        break;
      
      case ARROW_RIGHT:
        if(E.mode == "Normal" && !E.forward_stack.empty()){
          E.back_stack.push(E.cur_dir);
          string next_dir_path = E.forward_stack.top();
          E.forward_stack.pop();
          getallfiles(next_dir_path,E.files);
        }
        break;
      
      case ARROW_LEFT:
        if(E.mode == "Normal" && !E.back_stack.empty()){
           E.forward_stack.push(E.cur_dir);
          string prev_dir_path = E.back_stack.top();
          E.back_stack.pop();
          getallfiles(prev_dir_path,E.files);
        }
        break;

      case key_h:
        E.back_stack.push(E.cur_dir);
        getallfiles(E.root,E.files);
        break;
      
      case 127:   ///When backspace is pressed
        if(E.cur_dir != E.root){
          E.back_stack.push(E.cur_dir);
          for(int index = 0; index<E.files.size(); index++){
            if(E.files[index].name ==  ".."){
              getallfiles(E.files[index].path,E.files);
              break;
            }
          }
        }
        break;

      case key_r:
        if (E.mode == "Normal" && E.r != E.files.size() - 1){
          E.l = E.r + 1;
          E.files_index = E.l;
          E.r = min(E.r + E.jump, int(E.files.size()) - 1);
          editorRefreshScreen();
          printdir(E.l, E.r, E.files);
        }
        break;

      case key_l:
        if (E.mode == "Normal" && E.l != 0){
          E.r = E.l - 1;
          E.l = max(E.l - E.jump, 0);
          E.files_index = E.l;
          editorRefreshScreen();
          printdir(E.l, E.r, E.files);
        }
        break;

      case Enter:
        if(E.mode == "Normal"){ //check if the current entry is a directory
          if (E.files[E.files_index].type == "d"){
            E.back_stack.push(E.cur_dir); // Push the curr dir to stack before moving some. else
            getallfiles(E.files[E.files_index].path, E.files);
          }
          else if(E.files[E.files_index].type == "-"){
            string file_path = E.files[E.files_index].path;
            
            char *exec_args[] = {strdup("/usr/bin/vi"), strdup(file_path.c_str()), NULL};
            int childProcess = fork();
            if(childProcess == 0){ // Open VI In child
              execv("/usr/bin/vi", exec_args);
            }
            else{
              wait(&childProcess);
              printdir(E.l, E.r, E.files);
            }
          }
        }
        break;
      
      case 58: // for colon
        initCommandmode();
        break;
      
      default:
        break;
    }
  }
  else if(E.mode == "Command"){
    if(c==27){ //When pressing escape
      E.mode = "Normal";
      E.files_index  = E.l;
      printdir(E.l,E.r,E.files);
    }
    else if(c==127){ //Backspace
      if(!E.command_string.empty()) E.command_string.pop_back();
      write(STDOUT_FILENO, "\033[1D", 4);
      write(STDOUT_FILENO, "\033[0K", 4);
    }
    else if(c==1004){ //Enter
        write(STDOUT_FILENO,"\n\r",2);
        execute_command();
        E.command_string = "";
    }
    else{
      char key = char(c);
      E.command_string.push_back(key);
      write(STDOUT_FILENO,&key,1);
      //write(STDOUT_FILENO,"\x1b[1C",4);
    }
    
  }
}

//initializes the editor on startup
void initEditor(){
    enableRawMode();

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    
    E.mode = "Normal";
    
    //int number = getallfiles(getAbsPath("Terminal-File-Explorer-master"), E.files);
    int number = getallfiles(E.root, E.files);
    
    E.count = number;
}


// initializes the command mode
void initCommandmode(){
  E.mode = "Command";
  E.files_index = E.l;
  printdir(E.l,E.r,E.files); //Print everything again and ignore command mode printing
  
  E.command_string = "";
  string cursor_pos = to_string(E.screenrows-1);
  string reposition = "\x1b[" + cursor_pos + ";1H";
  write(STDOUT_FILENO,reposition.c_str(),8); //bring the cursor to bottom
  
  string status_bar = ":Command Mode create_file create_dir delete_file delete_dir rename copy move search goto\r\n";
  cout << status_bar;
}

int main(){

  initEditor(); 
  signal(SIGWINCH, winsz_handler); //generates interrupt whenever you resize the terminal
  while (1) {
    editorProcessKeypress();
  }
  return 0;
}

// gives a string of tokens of the command entered by user
vector<string> get_tokens(string command){
    vector <string> tokens; 
      
    // stringstream class check1 
    stringstream check1(command); 
      
    string intermediate; 
      
    // Tokenizing w.r.t. space ' ' 
    while(getline(check1, intermediate, ' ')){ 
        tokens.push_back(intermediate); 
    }
    return tokens;
}

//check whether file is dir. or not
bool isDirectory(string path){
    struct stat stat_ptr;
    if(stat(path.c_str(), &stat_ptr) != 0) {
        perror(path.c_str());
        return false;
    }   
    if(S_ISDIR(stat_ptr.st_mode)){
        return true;
    }
    else{
        return false;
    }
}

//Executes various commands based on what user has typed
void execute_command(){
    vector<string> command_arguments = get_tokens(E.command_string);
    string command = command_arguments[0];
    if(command == "create_file"){
      create_file(command_arguments);
    }
    else if(command == "create_dir"){
      create_dir(command_arguments);
    }
    else if(command == "copy"){
      copy_rec(command_arguments);
    }
    else if(command == "copy"){
      copy_rec(command_arguments);
    }
    else if(command == "move"){
      move(command_arguments);
    }
    else if(command == "rename"){
      rename(command_arguments);
    }
    else if(command == "delete_file"){
      delete_file(command_arguments);
    }
    else if(command == "delete_dir"){
      delete_dir(command_arguments);
    }
    else if(command == "search"){
      search(command_arguments);
    }
    else if(command == "goto"){
      go_to(command_arguments);
    }
    else{
      cout <<"You have typed the wrong command/r/n";
    }
    return;
}

//Processes the path according to path_string provided by user
string process_path_string(string path){
  if(path[0] == '.' ){
    return "";
  }
  if(path[0] == '~'){
    return path.substr(2);
  }
  return path;
}

//Refreshes the vector storing file attributes
void refresh_vector_file(vector<struct f> &files){
  
  string s = E.cur_dir;
  struct dirent *dirent_ptr;      // Pointer for directory entry
    
  int count = 0;  //count the number of directories and files
  DIR *dr = opendir(s.c_str()); // opendir() returns a pointer of DIR type.

  if (dr == NULL){  // opendir returns NULL if couldn't open directory
    cout << "Could not open current directory";
    return;
  }
  
  files.clear(); //clear the vector of files before inserting new ones
  while ((dirent_ptr = readdir(dr)) != NULL){
            struct stat stat_ptr;
            struct f file_info;
            count++;
            string abs_path_of_file = s + "/" + dirent_ptr->d_name;
            stat(abs_path_of_file.c_str(), &stat_ptr);
            
            file_info.name = dirent_ptr->d_name;
            file_info.size = to_string(stat_ptr.st_size);
            file_info.path =  abs_path_of_file;
            file_info.type = (S_ISDIR(stat_ptr.st_mode)) ? "d" : "-";
            file_info.uid = (!getpwuid(stat_ptr.st_uid))?"":getpwuid(stat_ptr.st_uid)->pw_name;
            file_info.gid = (!getgrgid(stat_ptr.st_gid))?"":getgrgid(stat_ptr.st_gid)->gr_name;
            file_info.permissions = getPermission(stat_ptr);
            file_info.lmodified = ctime(&stat_ptr.st_mtime);
            file_info.lmodified = file_info.lmodified.substr(0, file_info.lmodified.size() - 1);

            
            files.push_back(file_info);
    }
}

void create_file(vector<string> &tokens){
    string dest_folder = E.root + "/" + process_path_string(tokens[tokens.size()-1]);
    if(tokens.size() < 3){
        cout << "3 arguments are needed\n\r" << endl;
        return;
    }
    else{
      //verifies if destination is directory or not.
      if(!isDirectory(dest_folder)){
        cout << "Destination is not directory\n\r" << endl;
          return;
      }
    }
    
    FILE* create_file;
    
    string dest_path = dest_folder + "/" + tokens[1];
    
    create_file = fopen(dest_path.c_str(), "w+");
    if(create_file == NULL){
      perror("");
    }
    else{
      cout << "file created\n\r" << endl;
      fclose(create_file);

      refresh_vector_file(E.files);
    }
    return;
}

void create_dir(vector<string> &tokens){ 
   string dest_folder = E.root + "/" + process_path_string(tokens[tokens.size()-1]);
   if(tokens.size() < 3){
        cout << "3 arguments are needed\r\n";
        return;
   }
   else{
    //verifies if destination is directory or not.
     if (!isDirectory(dest_folder)){
       cout << "Destination is not directory.\n\r" << endl;
       return;
     }
     string dest_path = dest_folder + "/" + tokens[1];
     if(mkdir(dest_path.c_str(), 0755) != 0){
       perror("");
     }
     else{
       cout << "directory created successfully.\n\r" << endl;
       refresh_vector_file(E.files);
     }
   }
   return;
}

void copy_file(string source_path, string dest_path){ // This funcn takes absolute paths only
  char buf[BUFSIZ];
  size_t size;
  FILE *source_f, *dest_f;
    
  if((source_f = fopen(source_path.c_str(), "rb")) == NULL){
    perror("");
    cout << "\r\n";
    return;
  }
  if((dest_f = fopen(dest_path.c_str(), "wb")) == NULL){
    perror("");
    cout << "\r\n";
    return;
  }
  while(size = fread(buf, 1, BUFSIZ, source_f)){
    fwrite(buf, 1, size, dest_f);
  }
  struct stat source_stat;
  stat(source_path.c_str(), &source_stat);
  chown(dest_path.c_str(), source_stat.st_uid, source_stat.st_gid);
  chmod(dest_path.c_str(), source_stat.st_mode);
    
  fclose(source_f);
  fclose(dest_f);
  return;
}

void copy_dir(string source, string dest){ // This funcn takes absolute paths only
  DIR* dir_ptr;
  dir_ptr = opendir(source.c_str());
  struct dirent* d;
  if(dir_ptr == NULL){
    perror("opendir");
    return;
  }
  while((d = readdir(dir_ptr))){
    string name = d->d_name;
    if(name ==  "." || name ==  ".."){
      continue;
    }
    else{
      string source_path = source + "/" + string(d->d_name);
      string dest_path = dest + "/" + string(d->d_name);
      if(isDirectory(source_path)){
        if(mkdir(dest_path.c_str(), 0755) == -1){
          perror("");
          cout << "\r\n";
          return;
        }
        else{
          copy_dir(source_path, dest_path);
        }
      }
      else{
        copy_file(source_path.c_str(), dest_path.c_str());
      }
    }
  }
  closedir(dir_ptr);
  refresh_vector_file(E.files);
  return;
}

int copy_rec(vector<string> &tokens){
  if(tokens.size() < 3){
    cout << "too few arguments\r\n";
    return 0;
  }
  else{
    string dest_folder = E.root + "/" + process_path_string(tokens[tokens.size() - 1]);
    if(!isDirectory(dest_folder)){
      cout << "Destination is not a folder.\r\n" << endl;
      return -1;
    }
    for(int i = 1; i < tokens.size() - 1; i++){
      string source_path = getAbsPath(tokens[i]);
      int found = source_path.find_last_of("/\\");
      string dest_path = dest_folder + "/" + source_path.substr(found + 1, source_path.length() - found);
      
      if(isDirectory(source_path)){
        if(mkdir(dest_path.c_str(), 0755) != 0){
          perror("");
          return -1;
        }
        copy_dir(source_path, dest_path);
          
      }
      else{
        copy_file(source_path, dest_path);
      }
    }
    refresh_vector_file(E.files);
    cout << "Copied\r\n";
  }
  return 0;
}

void delete_file(vector<string> &tokens){
  if(tokens.size() <2){
    cout << "Atleast two arguments are needed\n\r" << endl;
    return;
  }
  else{
    string source_path = E.root + "/" + process_path_string(tokens[tokens.size()-1]);
    if(remove(source_path.c_str()) != 0){
      perror("");
      cout << "\r\n";
    }
    else{
      cout << "file Deleted\r\n";
      refresh_vector_file(E.files);
    }
  }
  return;
}

void delete_dir(vector<string> &tokens){ //this will call delete_recursively
    if (tokens.size() < 2){
      cout << "Atleast 2 arguments\r\n" << endl;
      return;
    }
    else{
      string path = E.root + "/" + process_path_string(tokens[tokens.size()-1]);
      if(!isDirectory(path)){
        cout << "Not a Directory\r\n" << endl;
      }
      delete_rec(path);
      refresh_vector_file(E.files);
      cout << "Deleted\r\n";
    }
    return;
}

void delete_rec(string path_to_delete){
  DIR* dir_ptr;
  dir_ptr = opendir(path_to_delete.c_str());
  struct dirent* d;
  if(dir_ptr == NULL) {
      perror("opendir");
      return;
  }
  while((d = readdir(dir_ptr))){
    string name = d->d_name;
    if(name == "." || name == ".."){
      continue;
    }
    else{
      string final_path = path_to_delete + "/" + string(d->d_name);
      if(isDirectory(final_path)){
        delete_rec(final_path);
      }
      else{
        remove(final_path.c_str());
      }
    }
  }
  rmdir(path_to_delete.c_str());
  closedir(dir_ptr);
  return;
}

void move(vector<string> &tokens){
  copy_rec(tokens);
  if(tokens.size() < 2){
    cout << "too few arguments\n\r";
    return;
  }
  else{
    for(int i = 1; i < tokens.size()-1; i++){
      string source_path = getAbsPath(tokens[i]);
      if(isDirectory(source_path)){
          delete_rec(source_path);
      }
      else{
        if(remove(source_path.c_str()) == -1){
          perror("");
        }
      }
    }
    cout << "moved\r\n";
  }
  return;
}

void rename(vector<string> &tokens){
  if (tokens.size() < 3){
    cout << "Improper Arguments\n\r";
  }  
  else{
    string old_name = getAbsPath(tokens[1]);
    string new_name = getAbsPath(tokens[2]);
    if (rename(old_name.c_str(), new_name.c_str()) == -1){
      perror("");
    }
    else{
      cout << "renamed\r\n";
      refresh_vector_file(E.files);
    }
    return;
  }
}

void search(vector<string> &tokens){
  if(tokens.size() < 2){
    cout << "2 arguments needed\n\r" << endl;
  }
  else{
    string name = tokens[tokens.size()-1];
    if(search_rec(E.cur_dir,name)){
      cout << "True\r\n" << endl;
      return;
    }
  }
  cout << "False\r\n" << endl;
  return;
}

bool search_rec(string search_path, string name){
  DIR* dir_ptr;
  dir_ptr = opendir(search_path.c_str());
  struct dirent* d;
  if (dir_ptr == NULL){
    perror("opendir");
    return false;
  }
  while((d = readdir(dir_ptr))){
    string dir_name = d->d_name;
    if(dir_name ==  "." || dir_name == ".."){
      continue;
    }
    else{
      string new_path = search_path + "/" + string(d->d_name);
      if(strcmp(d->d_name, name.c_str()) == 0){
        return true;
      }
      if(isDirectory(new_path)){
        if(search_rec(new_path, name)){
         return true;
        }
      }
    }
  }
  
  return false;
}

void go_to(vector<string> &tokens){
  if(tokens.size() < 2){
    cout << "2 arguments are needed\r\n";
  }
  else{
    string dest = E.root + "/" + process_path_string(tokens[tokens.size()-1]);
    E.back_stack.push(E.cur_dir);
    getallfiles(dest,E.files);
    initCommandmode();
  }
}


//https://stackoverflow.com/questions/32142164/how-to-use-execv-system-call-in-linux
// see why when typing wrong command we are not getting error

