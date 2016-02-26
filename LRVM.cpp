#include "LRVM.h"
#include <string.h>
#include "malloc.h"

map_t segments;
map_trans rvmmap;
map_seg map_segname;
redo_log_t redo_log;
undo_log_t undo_log;
trans_t trans_count=0;
directory_t directories;
static bool flag;
map_segment_t map_segment;

rvm_t rvm_init(const char *directory)
{
  cout<<"flag="<<flag<<endl;
  flag=true;
  if(directories.count(directory)==0)
  {
    string str1="mkdir ";
    string str2(directory,strlen(directory));
    string str;
    str.append(str1);
    str.append(str2);
    char* c_str=(char*)malloc(str.length());
    strcpy(c_str,str.c_str());
    system((const char*)c_str);
    directories[directory]=directory;
    return directory;
  }
  else 
  {
    cout<<"directory already present"<<endl;
    return directory;
  }
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
  cout<<"entering map function"<<endl;
  int filesize=0;
  string dir(rvm);
  string filname(segname);
  string separator = "/";
  string full_path = dir + separator + filname;
  ifstream input;
  input.open((const char*)full_path.c_str(),ios::binary);
  if(input.good())
  {
    input.seekg(0,input.end);
    filesize=input.tellg();
  }   
  else filesize=-1;
  input.close();
  cout<<"filesize="<<filesize<<endl;

//see if the segment exists or not
  if(filesize!=-1)
  {
    //segment exits on disk 

    //check if segment is in map or not
    if(segments.count(segname)>0)
    {
      cout<<"Segment "<<segname<<"is in map"<<endl;
      //segment is in map
      //check if file_size=size_to_create if not realloc

      map_t::iterator it=segments.begin();
      it=segments.find(segname);
      cout<<"before"<<it->second.size<<endl;
      if(it->second.size<size_to_create)
      {
        cout<<"reallocating"<<endl;
        it->second.address=(char*)realloc(it->second.address,sizeof(size_to_create));
        it->second.size=size_to_create;
        it->second.in_use=false;
        cout<<"afer reallocation"<<it->second.size<<endl;
        map_segname.insert(pair<void*,const char*>(it->second.address,segname));
        return (void*) it->second.address;
      }
      else 
      {
        cout<<"Error:already mapped"<<endl;
        return NULL;//it is an error to map to map to the same segment twice
      }
    }
    else  
    {
      //segment is in not the map put the segment in the map
      truncate_segment(rvm,segname);
      cout<<"Segment "<<segname<<" not in map"<<endl;
      segment_t segment;
      segment.address=(char*)malloc(size_to_create);
      segment.size=size_to_create;
      segment.in_use=false;
      //copy the contents of the file on to the segment
      ifstream infile((const char*) full_path.c_str(),ios::binary);
      cout<<"path="<<full_path<<endl;
      string buffer;
      while(getline(infile,buffer));
      memcpy(segment.address,(char*) buffer.c_str(), buffer.length());
      segments.insert(pair<const char*,segment_t>(segname,segment));
      map_segname.insert(pair<void*,const char*>(segment.address,segname));
      cout<<"Segment "<<segname<<" has been put in map"<<endl;
      return (void*) segment.address;
    }
  }
  else
  {
    cout<<"no such segment exists "<<endl;
    //segment does not exist create segment then put in the map
    ofstream out;
    out.open((const char*) full_path.c_str(),ios::binary);
    out.seekp((size_to_create)-1);
    out.write("",1);
    out.close();
    segment_t segment;
    segment.address=(char*)malloc(size_to_create);
    segment.size=size_to_create;
    segment.in_use=false;
    segments.insert(pair<const char*,segment_t>(segname,segment));
    map_segname.insert(pair<void*,const char*>(segment.address,segname));
    cout<<"segment "<<segname<<" created and put in map"<<endl;
    return (void*) segment.address;
  }
}


void rvm_unmap(rvm_t rvm, void *segbase)
{
  cout<<"entering unmap function"<<endl;
  if(segbase!=NULL)
  {
    const char* key=map_segname[segbase];
    if(segments.count(key)==0) cout<<"segment not present in the map so cannot be unmapped"<<endl;
    map_t::iterator it=segments.begin();
    it=segments.find(key);
    free(it->second.address);
    segments.erase(it);
    cout<<"segment "<<key<<" removed"<<endl;
  }
}



trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)
{
  cout<<"entering begin function"<<endl;
  trans_count++;
  rvmmap[trans_count]=rvm;
  for(int i=0;i<numsegs;i++)
  {
    const char* key=map_segname[segbases[i]];
    if(segments.count(key)==0)
    {
      //segment not in the map
      cout<<"Segment not mapped "<<endl;
      return -1;
    }
    else
    {
      //segment is in the map
      map_t::iterator it=segments.begin();
      it=segments.find(key);
      //check if already in use or not
      if(it->second.in_use==true)
      {
        return -1;
      }
      else
      {
        it->second.in_use=true;
      }
    }
    map_segment[trans_count].push_back(key);
    cout<<"segment "<<key<<" is ready to be modified"<<endl;
  }
  return trans_count;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
  cout<<"entering about_to_modify function"<<endl;
  if(rvmmap.count(tid)>0)
  {
    //the rvm has been called by the user to begin translation
    //create the undo record
    
    undo_t* undo_record=(undo_t*)malloc(sizeof(undo_t));
    undo_record->size=size;
    undo_record->offset=offset;
    undo_record->segname=map_segname[segbase];
    undo_record->data=(char*)malloc(size);
    memcpy(undo_record->data,(char*)segbase+offset,size);
    char test[100];
    strcpy(test,(char*)segbase+offset);
		cout<<"test undo string="<<test<<endl;
    //now see if the undo record exists for that particular segment in the map or not
    cout<<"pointer="<<(char*)segbase+offset<<endl;
    if(undo_log.count(tid)>0)
    {
      //the undo record exists then just do push back
      undo_log_t::iterator it=undo_log.begin();
      it=undo_log.find(tid);
      it->second.push_back(undo_record);
      cout<<"pushing back undo logs"<<endl;
    }
    else
    {
      //create the new vector and insert it in the map
      vector<undo_t*> temp;
      temp.push_back(undo_record);
      undo_log[tid]=temp;
      cout<<"creating new undo logs"<<endl;
    }

    //create the redo logs
    redo_t* redo_record=(redo_t*)malloc(sizeof(redo_t));
    redo_record->segname=map_segname[segbase];
    redo_record->offset=offset;

    //now see if the redo record exists for that particular segment in the map or not
    if(redo_log.count(tid)>0)
    {
      //the redo record exists then just do push back
      redo_log_t::iterator it=redo_log.begin();
      it=redo_log.find(tid);
      it->second.push_back(redo_record);
      cout<<"pushing back redo logs"<<endl;
    }
    else
    {
      //create the new vector and insert it in the map
      vector<redo_t*> temp;
      temp.push_back(redo_record);
      redo_log[tid]=temp;
      cout<<"creating new redo logs"<<endl;
    }
    return;
  }
  else cout<<"Invalid action"<<endl;
}


void rvm_destroy(rvm_t rvm, const char* segname)
{
  cout<<"entering destroy function"<<endl;
  if (segments.count(segname)>0)
  {
	  cout << "The segment is mapped. Cannot delete.";
	  return;
  }
  string dir(rvm);
  string fil(segname);
  string separator = "/";
  string deleted_name = dir + separator + fil;
  char* command = (char*) malloc(strlen(deleted_name.c_str()) + 4);
  strcpy(command, "rm ");
  strcat(command, deleted_name.c_str());
	system(command);
	}

void rvm_commit_trans(trans_t tid)
{
	cout << "\nInside commit transaction" << endl;
  if (redo_log.count(tid)!=0)
	{
		vector<redo_t*> redo_recs;
		redo_recs = redo_log[tid];
		for (vector<redo_t *>::iterator it = redo_recs.begin();it != redo_recs.end(); ++it)
		{
			redo_t *r = new redo_t();
			r = *it;
			char *segname = (char *) (r->segname);
      if(segments.count(r->segname)>0)
      {
        map_t:: iterator mt=segments.find(r->segname);
        mt->second.in_use=false;
			}
      r->new_data = (char*)segments[segname].address + r->offset;
      char test[100];
      strcpy(test,(const char*)r->new_data);
		  cout<<"test string="<<test<<endl;
		  cout<<"offset="<<r->offset<<endl;
		  string dir(rvmmap[tid]);
		  string fil(segname);
		  string separator = "/";
		  string seglog= dir + separator + fil +".log";
	    ofstream segfile((char *) seglog.c_str(), ios::out | ios::app);
	  	segfile << r->offset << "|" << r->new_data << endl;
  		segfile.close();
		}
		undo_log.erase(tid);
		redo_log.erase(tid);
		rvmmap.erase(tid);
	}
	else
	{       
	  vector<const char*> key;
    cout<<"id="<<tid<<endl;
    key=map_segment[tid];
    for(vector<const char*>::iterator it=key.begin();it!=key.end();it++)
    {
      map_t:: iterator mt=segments.find(*it);
      mt->second.in_use=false;
    }
		cout << "\nIncorrect Transaction ID during commit" << endl;
	}
}

void rvm_abort_trans(trans_t tid)
{
cout<<"entering abort function"<<endl;
	if (undo_log.count(tid)!=0)
	{
		vector<undo_t*> undo_recs;
		undo_recs = undo_log[tid];
		undo_t *u = new undo_t();
		for (vector<undo_t*>::iterator it = undo_recs.begin();it != undo_recs.end(); ++it)
		{
			u = *it;
			char *segname = (char *) u->segname;
      if(segments.count(u->segname)>0)
      {
        map_t:: iterator mt=segments.find(u->segname);
        mt->second.in_use=false;
			}
			char *data = (char*)u->data;
			memcpy(segments[segname].address + u->offset, data, u->size);
      char test[100];
      const char* new_data1=(const char*)u->data;
			const char* new_data=(const char*)segments[segname].address + u->offset;
      strcpy(test,new_data);
      cout<<"test abort string="<<new_data1<<endl;
			cout<<"test abort string="<<test<<endl;
		}
		undo_log.erase(tid);
		redo_log.erase(tid);
		rvmmap.erase(tid);
	}
	else
	{
    vector<const char*> key;
    cout<<"id="<<tid<<endl;
    key=map_segment[tid];
    for(vector<const char*>::iterator it=key.begin();it!=key.end();it++)
    {
      map_t:: iterator mt=segments.find(*it);
      mt->second.in_use=false;
    }
		cout << "\nIncorrect Transaction ID in Abort" << endl;
	}
}

void rvm_truncate_log(rvm_t rvm)
{
  cout<<"entering truncate function"<<endl;
	DIR *d;
	struct dirent *dir;
	vector<string> dirlist;
	d = opendir(("./"+string(rvm)).c_str());
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			string fname(dir->d_name);
			if (fname.find(".log") != string::npos)
			{
				dirlist.push_back(dir->d_name);
			}
  	}
		closedir(d);
	}
	for (vector<string>::iterator it = dirlist.begin(); it != dirlist.end();++it)
	{
		string str = *it;
		str = str.substr(0, str.length() - 4);
		char *segname = (char *) str.c_str();
		truncate_segment(rvm, segname);
	}
}

void truncate_segment(rvm_t rvm, const char* segname)
{
	//Truncates the segname log file in rvm_t directory
  cout<<"inside truncate segment function"<<endl;	
	string dir(rvm);
	string fil(segname);
	string separator = "/";
	string path = dir + separator + fil;
  string seglog=path + ".log"; 
  cout<<"seglog="<<seglog<<endl;
	string buffer;
	int offset;
	char *data, *token;
	string command = "rm " + path + ".log";
  int filesize=0;	
  ifstream input;
  input.open((const char*)seglog.c_str(),ios::binary);
  if(input.good())
  {
    input.seekg(0,input.end);
    filesize=input.tellg();
  }
  else filesize=-1;
  if (filesize > -1)
	{
		ifstream inputfile((char*) (path+".log").c_str());
		if (inputfile.is_open())
		{
			while (getline(inputfile, buffer))
			{
				token = strtok((char *) buffer.c_str(), "|");
				offset = atoi(token);
				token = strtok(NULL, "|");
				data = token;
				cout<<"\nOffset: "<<offset<<"\nData: "<<data<<endl;
        FILE * segFile;
				segFile = fopen((char *) path.c_str(), "r+");
				fseek(segFile, offset, SEEK_SET);
				fwrite(data, sizeof(char),(string(data)).length(),segFile);
				fclose(segFile);
			}
			inputfile.close();
		}
		system((char *) command.c_str());
	}
	else
	{      
		system((char *) command.c_str());
	}
}




