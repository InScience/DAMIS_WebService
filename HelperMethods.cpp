///////////////////////////////////////////////////////////
//  HelperMethods.cpp
//  Implementation of the Class HelperMethods
//  Created on:      30-Sau-2014 17:06:18
//  Original author: Povilas
///////////////////////////////////////////////////////////

#define _ELPP_THREAD_SAFE
#define _ELPP_STL_LOGGING
#define _ELPP_NO_DEFAULT_LOG_FILE

#include "HelperMethods.h"
#include "ServiceSettings.h"

#include "logging/easylogging++.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <cstring>
#include <numeric>
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

const char* HelperMethods::alphanum= "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";


HelperMethods::HelperMethods(){

}

HelperMethods::~HelperMethods(){

}

double HelperMethods::getAttributeValue(std::string statFile, std::string attName)
{
double retVal = 0.0; // was uninitialised: a missing stat file returned garbage (e.g. 9.27e-310)
std::string attribute = attName;
std::vector<std::string> tmp;
tmp.reserve(0);
std::string line_from_file, tmp1;

std::transform(attribute.begin(), attribute.end(), attribute.begin(), ::toupper);

int tmpIndex = -1, attIndex = 0;

std::ifstream file (statFile);

if (file.is_open() != false)
    {
        while (std::getline(file, line_from_file))
        {
            if (line_from_file.length() == 0)
            {
                continue;
            }
            std::istringstream iss(line_from_file);
            std::string sub;
            iss >> sub;
            if (sub == "%")
            {
                continue;
            }
            else
            {
                std::transform(sub.begin(), sub.end(), sub.begin(), ::toupper);
                if (sub == "@ATTRIBUTE")
                {
                    tmpIndex ++;
                    iss >> tmp1;
                    //iss >> tmp2;

                    std::transform(tmp1.begin(), tmp1.end(), tmp1.begin(), ::toupper);
                   // std::transform(tmp2.begin(), tmp2.end(), tmp2.begin(), ::toupper);
                    if (tmp1 == attribute)
                    {
                        attIndex = tmpIndex;
                    }
                }
                else if (sub == "@DATA" || sub == "@RELATION")
                {
                    continue;
                }
                else
                {
                    tmp = HelperMethods::split(line_from_file, ','); //split data section by comma
                    const char *str = tmp.at(attIndex).c_str();
                    char *err;
                    double x = strtod(str, &err);
                    if (*err == 0 && tmp.at(attIndex) !="")
                    {
                      retVal = atof(tmp.at(attIndex).c_str());
                    }
                }
            }
        }
      //  file.close();
    }
    file.close();
return retVal;
}


/**
 * Method generates random file name and adds to the prefix
 */
std::string HelperMethods::generateFileName(std::string preffix, bool addExtension)
{

    LOG(INFO) << "Starting file name generation with the preffix: " << preffix;

    // std::random_device instead of srand(time): requests served in the same second used to get
    // identical file names and overwrite each other's input/output files
    static std::random_device rd;
    static std::mt19937 gen(rd() ^ (static_cast<unsigned int>(getpid()) << 16));
    std::uniform_int_distribution<size_t> pick(0, strlen(HelperMethods::alphanum) - 1);

    std::string catString;
    catString.assign(preffix);

    int i;
    int qty = catString.length() + ServiceSettings::lenghtOfTmpFileName;
    for ( i = catString.length(); i < qty; ++i)
        catString += HelperMethods::alphanum[pick(gen)];

    if (addExtension)
        catString += ServiceSettings::dataFileExtension;

    LOG(INFO) << "Generated file name: " << catString;

    return  catString;
}

/**
 * Seconds since an arbitrary fixed point (monotonic wall clock), for request durations.
 * clock() used before measures CPU time of this process only, not the time spent waiting for mpirun.
 */
double HelperMethods::wallTime()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}


/**
 * Method that checks and deletes old files from specified directory (provided in ServiceSettings file)
 */
int HelperMethods::deleteOldFiles()
{
    LOG(INFO) << "Initiating old file delete check routine";
    DIR *dir;
    struct dirent *ent;
    int fileCount = 0;
    time_t nowTime = time(NULL);                     // get local time

    struct stat attrib;                 // create a file attribute structure
    std::string fPath;                  // tmp construct path to all files

    LOG(INFO) << "Opening file storage directory: " << ServiceSettings::localDataFileSavePath.c_str();

    if ((dir = opendir (ServiceSettings::localDataFileSavePath.c_str())) != NULL)
    {
        /* get all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL)
        {
            fPath = ServiceSettings::localDataFileSavePath + ent->d_name;
            stat(fPath.c_str(), &attrib);         // get the attributes of a file

            //if it is not directory and time has ellapsed DELETE FILE
            if ((attrib.st_mode & S_IFMT) != S_IFDIR && difftime(nowTime, attrib.st_mtime) > ServiceSettings::keepFilesInDirectory)
            {
                fileCount++;
                LOG(INFO) << "Preparing to detele file - " << fPath.c_str();
                if (std::remove(fPath.c_str()) != 0)
                    LOG(WARNING) << "Error deleting file - "<< fPath.c_str();
                else
                    LOG(INFO) << "File " << fPath.c_str() << " was deleted";
            }
        }
        closedir (dir);
    }
    else
    {
        /* could not open directory */
        LOG(WARNING) << "Unable to open file storage directory " << ServiceSettings::localDataFileSavePath;
    }

    LOG(INFO) << fileCount <<" deleted files in total";
    return fileCount;
}

/**
 * Method that deletes single file passed as a parameter
 */
int HelperMethods::deleteFile(std::string path)
{

    LOG(INFO) << "Preparing to detele file: "  << path;

    if (std::remove(path.c_str()) != 0)
        LOG (WARNING) << "Error deleting file: " << path <<". Another try to delete file will be after " << ServiceSettings::keepFilesInDirectory << " seconds of service invocation";
    else
        LOG(INFO) <<"File deleted: " << path;

return 0;
}

std::vector<std::string> HelperMethods::split(const std::string& s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> HelperMethods::split(const std::string& s, char delim)
{
    std::vector<std::string> elems;
    HelperMethods::split(s, delim, elems);
    return elems;
}


int HelperMethods::getRrandomInRange(unsigned int min, unsigned int max)
{
    // rejection sampling; was: int p = RAND_MAX+1 (int overflow) which skipped the loop and
    // always returned the same "random" index, so SPLITDATA reshuffling was not random
    int x, randMax = RAND_MAX - RAND_MAX % max;
    int p;
    do
        p = rand();
    while (p >= randMax);
    x = min + p % (max - min);
    return x;
}

double HelperMethods::getMean(std::vector<double> dataVector)
{
    return std::accumulate(dataVector.begin(), dataVector.end(), 0.0) / dataVector.size();
}

double HelperMethods::getStd(std::vector<double> dataVector)
{
    std::vector<double> diff(dataVector.size());
    const double mean = HelperMethods::getMean(dataVector);
    std::transform(dataVector.begin(), dataVector.end(), diff.begin(), [mean](double v) { return v - mean; });
    double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    return std::sqrt(sq_sum / dataVector.size()); //daliname is N o ne is N-1
}

double HelperMethods::strToDouble(std::string cmdParam)
{
    const char *str = cmdParam.c_str();
    char *err;
    double x = strtod(str, &err);
    if (*err == 0 && cmdParam !="")
    {
        return atof(cmdParam.c_str());
    }
    return 0.0; // used to fall off the end (undefined behaviour)
}

int HelperMethods::strToInt(std::string cmdParam)
{
    const char *str = cmdParam.c_str();
    char *err;
    double x = strtod(str, &err);
    if (*err == 0 && cmdParam !="")
    {
        return atoi(cmdParam.c_str());
    }
    return 0; // used to fall off the end (undefined behaviour)
}

// copy in binary mode
bool HelperMethods::copyFile(const char *SRC, const char* DEST)
{
    std::ifstream src(SRC, std::ios::binary);
    std::ofstream dest(DEST, std::ios::binary);
    dest << src.rdbuf();
    return src && dest;
}


