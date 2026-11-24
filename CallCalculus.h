///////////////////////////////////////////////////////////
//  CallCalculus.h
//  Implementation of the Class CallCalculus
//  Created on:      24-Vas-2014 16:29:02
//  Original author: Povilas
///////////////////////////////////////////////////////////

#if !defined(CALLCALCULUS_H)
#define CALLCALCULUS_H

#include "ServeRequest.h"

#include <string>
#include <vector>


 /** \brief Class implements the call of the algorithms (Algorithms binary started with mpirun on this host)
  *
  */

class CallCalculus : public ServeRequest
{

public:
    /** \brief Class constructor.
     *
     * \param int p number of processors.
     * \param int maxCalcTime predictive calculation time in seconds (informative only).
     * \param InitDamisService* initFile pointer to the InitDamisService object.
     *
     */
	CallCalculus(int p, int maxCalcTime, InitDamisService* initFile);
    /** \brief Class destructor
	*
	*/
	virtual ~CallCalculus();

protected:
    /** \brief Runs the algorithm: mpirun [mpiOptions] -np p Algorithms exeParams -i input -o outFile -s statFile
     *
     * Sets an ErrorResponse fault if the run fails, times out or does not produce the result files.
     * \return void
     */
	virtual void run();

	std::string exeParams; /**<  Holds the algorithm parameters, whitespace separated (e.g. " -al PCA -projType 0 -d 2") */
private:

    int reqProcessors; /**< Holds required processor count (mpirun -np) */

    int maxCalculationTime; /**< Predictive calculation time passed by the client (was used for cluster queue selection) */

    /** \brief Starts args[0] with the given arguments in its own process group and waits for it
     *
     * \param args program and its arguments (no shell is involved)
     * \param workDir working directory of the child process
     * \param logFile file that receives the child's stdout and stderr
     * \param timeoutSec kill the whole process group after this many seconds (0 = no limit)
     * \param exitCode exit code of the child (128 + signal number if it was killed by a signal)
     * \param timedOut set to true if the child was killed because of the timeout
     * \return false if the child could not be started
     */
    static bool runProcess(const std::vector<std::string>& args, const std::string& workDir, const std::string& logFile,
                           int timeoutSec, int& exitCode, bool& timedOut);
    /** \brief Returns the last lines of a text file (for error messages)
     */
    static std::string tailOfFile(const std::string& path, size_t maxLines);
    /** \brief true if the file exists and is not empty
     */
    static bool fileHasContent(const std::string& path);

};

#endif //!defined(CALLCALCULUS_H)
