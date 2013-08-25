#ifndef LOGGING_H
#define LOGGING_H

#include <fstream>
#include <ctime>

class logging
{
public:
	static void AppendLine(char* text);
	static void Append(char* text);
};

//logging::logging()
//{
//	_strdate(date);
//	ofstream File;               //Names File as ofstream (for output to file)
//	char filename[200];
//	sprintf(filename,"Logfile_%s.txt",date);
//	File.open(filename,ios::out);     //Opens "File.txt" for output
//	File << "Logfile created.\n";
//	File.close();
//}
void logging::AppendLine(char* text)
{
	char date[10];
	ofstream File;      
	_strdate(date);
	char filename[200];
	sprintf(filename,"Logfile_%s.txt",date);
	File.open(filename,ios::app);     //Opens "File.txt" for output
	File << text << "\n";
	File.close();
}
void logging::Append(char* text)
{
	char date[10];
	ofstream File;      
	_strdate(date);
	char filename[200];
	sprintf(filename,"Logfile_%s.txt",date);
	File.open(filename,ios::app);     //Opens "File.txt" for output
	File << text;
	File.close();
}
	
#endif //LOGGING_H    