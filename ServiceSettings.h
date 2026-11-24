///////////////////////////////////////////////////////////
//  ServiceSettings.h
//  Implementation of the Class ServiceSettings
//  Created on:      30-Sau-2014 17:06:18
//  Original author: Povilas
///////////////////////////////////////////////////////////

#if !defined(SERVICESETTINGS_H)
#define SERVICESETTINGS_H
#include <string>

/** \brief Class holds static attributes with the service settings
 *
 */
class ServiceSettings
{

public:
    /** \brief Constructor
     */
	ServiceSettings();
    /** \brief Destructor
     */
	virtual ~ServiceSettings();
     /** \brief Initilaizes runtime environment variables
      * \return bool success
      *
      */
	static bool initialize();

	static std::string localDataFileSavePath; /**< Local file path to data directory */

	static std::string httpDataFileSavePath; /**< Path to public HTTP data directory */

	static std::string dataFileExtension; /**< efault data file extension */

	static std::string logingConfFilePath; /**< Path to logging config file */

	 static int lenghtOfTmpFileName; /**< Lenght of the file name */

	 static int keepFilesInDirectory; /**< Time in seconds how lon files will be kept in data directory */

	 static std::string wsdlPath; /**< Path to wsdl file */

	 static int noOfProcessors; /**< Number of processors that may be used by the algorithms */

	 static std::string mpiOptions; /**< Extra mpirun options, whitespace separated (e.g. "--oversubscribe --bind-to none") */

	 static int maxRunTime; /**< Seconds after which a running algorithm is killed (keep below the Apache/PHP timeouts) */

	 static std::string pathToMPIExecutable; /**< mpirun executable (name in PATH or full path) */

	 static std::string workingDirMPI; /**< MPI working directory */

	 static std::string pathToAlgorithmsExe; /**< Path to cluster algorithms implementation exe */

private:

     /** \brief Returns EXE path
      * \return std::string of the execution
      *
      */
	 static std::string getExePath();
};
#endif //!defined(SERVICESETTINGS_H)
