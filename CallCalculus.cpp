///////////////////////////////////////////////////////////
//  CallCalculus.cpp
//  Implementation of the Class CallCalculus
//  Created on:      24-Vas-2014 16:29:02
//  Original author: Povilas
///////////////////////////////////////////////////////////
#define _ELPP_THREAD_SAFE
#define _ELPP_STL_LOGGING
#define _ELPP_NO_DEFAULT_LOG_FILE


#include "CallCalculus.h"
#include "ServiceSettings.h"
#include "ServeRequest.h"
#include "HelperMethods.h"
#include "ErrorResponse.h"
#include "logging/easylogging++.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

CallCalculus::CallCalculus(int p, int maxCalcTime, InitDamisService* initFile):ServeRequest(initFile)
{
    this->reqProcessors = p;
    this->maxCalculationTime = maxCalcTime; // informative only, the run is limited by ServiceSettings::maxRunTime
}

CallCalculus::~CallCalculus()
{

}

/**
 * Implements run of the algorithm.
 *
 * The VU MIF (SLURM sbatch) and MII (Grid Engine qsub) cluster submission that used to be here was
 * removed in 2026: the algorithms now run on the service host itself with mpirun.
 */
void CallCalculus::run()
{
    // create file for service calculations
    DamisFile *cFile = new DamisFile ("_forCalculus_");  // temporary data file that will be passed to the algorithm

    // save data to temporary file
    writeDataToFile(cFile->getFilePath(), prepareDataSection(serveFile->getDoubleData(), serveFile->getStringClass()), prepareAttributeSection(serveFile->getAttributeName(),serveFile->getAttributeType(), serveFile->getStringClassAttribute()));

    // mpirun [mpiOptions] -np p Algorithms <exeParams> -i input -o result -s stat (argument vector, no shell)
    std::vector<std::string> args;
    std::string token;
    args.push_back(ServiceSettings::pathToMPIExecutable);
    std::istringstream mpiOptions(ServiceSettings::mpiOptions);
    while (mpiOptions >> token)
        args.push_back(token);
    args.push_back("-np");
    args.push_back(std::to_string(reqProcessors > 0 ? reqProcessors : 1));
    args.push_back(ServiceSettings::pathToAlgorithmsExe);
    std::istringstream algorithmParams(exeParams);
    while (algorithmParams >> token)
        args.push_back(token);
    args.push_back("-i");
    args.push_back(cFile->getFilePath());
    args.push_back("-o");
    args.push_back(outFile->getFilePath());
    args.push_back("-s");
    args.push_back(statFile->getFilePath());

    std::string commandLine;
    for (size_t i = 0; i < args.size(); i++)
        commandLine += (i ? " " : "") + args[i];

    std::string runName = HelperMethods::generateFileName("", false);
    std::string runLog = ServiceSettings::workingDirMPI + "_run_" + runName + ".log";

    LOG(INFO) << "Running algorithm: " << commandLine;
    LOG(INFO) << "Algorithm output goes to " << runLog << ", time limit " << ServiceSettings::maxRunTime << " s (client maxCalcTime " << maxCalculationTime << " s)";

    int exitCode = -1;
    bool timedOut = false;
    double start = HelperMethods::wallTime();
    bool started = runProcess(args, ServiceSettings::workingDirMPI, runLog, ServiceSettings::maxRunTime, exitCode, timedOut);
    LOG(INFO) << "Algorithm process finished with exit code " << exitCode << " after " << (HelperMethods::wallTime() - start) << " s";

    if (!started)
    {
        LOG(ERROR) << "Unable to start a process for " << ServiceSettings::pathToMPIExecutable << ": " << strerror(errno);
        ErrorResponse::setFaultDetail("Unable to start the algorithm process. Token: " + runName);
    }
    else if (timedOut)
    {
        LOG(ERROR) << "Algorithm run exceeded the time limit of " << ServiceSettings::maxRunTime << " s and was stopped";
        ErrorResponse::setFaultDetail("Algorithm run exceeded the time limit of " + std::to_string(ServiceSettings::maxRunTime) + " s and was stopped. Token: " + runName);
    }
    else if (exitCode != 0 || !fileHasContent(outFile->getFilePath()) || !fileHasContent(statFile->getFilePath()))
    {
        std::string output = tailOfFile(runLog, 15);
        LOG(ERROR) << "Algorithm run failed (exit code " << exitCode << "): " << output;
        ErrorResponse::setFaultDetail("Algorithm run failed (exit code " + std::to_string(exitCode) + "). Token: " + runName);
        if (!output.empty())
            ErrorResponse::setFaultDetail(output);
    }

    if (!ErrorResponse::isFaultFound())
    {
        LOG(INFO) << "Job finished SUCCESSFULLY";
    }
    else
    {
        //copy input data and run log to err directory for further inspection
        LOG(INFO) << "Job FAILURE, copying data and log files to err directory";
        std::string errDir = ServiceSettings::workingDirMPI + "err";
        struct stat st;
        if (stat(errDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            HelperMethods::copyFile(cFile->getFilePath().c_str(), (errDir + "/" + cFile->getFileName()).c_str());
            HelperMethods::copyFile(runLog.c_str(), (errDir + "/_run_" + runName + ".log").c_str());
        }
    }

    HelperMethods::deleteFile(cFile->getFilePath());
    HelperMethods::deleteFile(runLog);
    delete cFile;
}

bool CallCalculus::runProcess(const std::vector<std::string>& args, const std::string& workDir, const std::string& logFile,
                              int timeoutSec, int& exitCode, bool& timedOut)
{
    timedOut = false;
    exitCode = -1;

    std::vector<char*> argv;
    for (size_t i = 0; i < args.size(); i++)
        argv.push_back(const_cast<char*>(args[i].c_str()));
    argv.push_back(nullptr);

    int logFd = open(logFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

    pid_t child = fork();
    if (child < 0)
    {
        if (logFd >= 0)
            close(logFd);
        return false;
    }

    if (child == 0)
    {
        // own process group: on timeout the whole group (mpirun and every rank) is killed
        setpgid(0, 0);
        // stdin of the CGI is the SOAP request and stdout is the SOAP response: keep the child away from both
        int devNull = open("/dev/null", O_RDWR);
        if (devNull >= 0)
            dup2(devNull, STDIN_FILENO);
        int outFd = logFd >= 0 ? logFd : devNull;
        if (outFd >= 0)
        {
            dup2(outFd, STDOUT_FILENO);
            dup2(outFd, STDERR_FILENO);
        }
        if (chdir(workDir.c_str()) != 0)
            fprintf(stderr, "Unable to change directory to %s: %s\n", workDir.c_str(), strerror(errno));
        // Apache starts CGI scripts without HOME, mpirun (PRRTE) refuses to run without it
        const char *home = getenv("HOME");
        if (!home || !*home)
            setenv("HOME", "/tmp", 1);
        const char *tmp = getenv("TMPDIR");
        if (!tmp || !*tmp)
            setenv("TMPDIR", "/tmp", 1);
        execvp(argv[0], argv.data());
        fprintf(stderr, "Unable to start %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    setpgid(child, child); // also from the parent, avoids a race with the child's own call
    if (logFd >= 0)
        close(logFd);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec > 0 ? timeoutSec : 0);
    int status = 0;
    for (;;)
    {
        pid_t r = waitpid(child, &status, WNOHANG);
        if (r == child)
            break;
        if (r < 0 && errno != EINTR)
            return false;

        if (timeoutSec > 0 && std::chrono::steady_clock::now() >= deadline)
        {
            timedOut = true;
            kill(-child, SIGTERM);
            for (int i = 0; i < 50 && waitpid(child, &status, WNOHANG) == 0; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            kill(-child, SIGKILL);
            waitpid(child, &status, 0);
            break;
        }
        // the old cluster wait loop polled without sleeping (100 % CPU)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (WIFEXITED(status))
        exitCode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        exitCode = 128 + WTERMSIG(status);

    return true; // exit code 127 = exec failed, the reason is in the log file
}

std::string CallCalculus::tailOfFile(const std::string& path, size_t maxLines)
{
    std::ifstream file(path);
    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        lines.push_back(line);
        if (lines.size() > maxLines)
            lines.pop_front();
    }
    std::string result;
    for (size_t i = 0; i < lines.size(); i++)
        result += (i ? "\n" : "") + lines[i];
    return result;
}

bool CallCalculus::fileHasContent(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}
