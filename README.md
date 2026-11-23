DAMIS_WebService
================

SOAP web service of [DAMIS](https://github.com/InScience/DAMIS-GUI): `DamisService.cgi`, a
[gSOAP](https://www.genivia.com/) server that runs as an Apache CGI. The DAMIS-GUI calls it with PHP
`SoapClient` (cron command `experiment:execute`).

For every request the service:

1. downloads the input dataset `X` (an ARFF URL) with libcurl and validates it and the parameters;
2. runs the operation:
   - data operations `CLEANDATA`, `FILTERDATA`, `NORMDATA`, `SPLITDATA`, `TRANSPOSEDATA`, `STATPRIMITIVES`
     inside the CGI,
   - algorithms `PCA`, `SMACOFMDS`, `DMA`, `RELMDS`, `SAMANN`, `SOMMDS`, `SOM`, `KMEANS`, `MLP`, `DF` with
     `mpirun -np p Algorithms …` (the [DAMIS algorithms](https://github.com/InScience/DAMIS) binary);
3. answers with `Y` (URL of the result file, `Yalt` for the second SPLITDATA part), `calcTime` and, for
   algorithms, `algorithmError`. Errors are SOAP faults (`Damis error` + description).

The interface is [`wsdl/damis.wsdl`](wsdl/damis.wsdl) (served on `?wsdl`). PHP `SoapClient` maps call
arguments to the message parts by position, gSOAP reads them by name: keep both order and names stable.

Docker (recommended)
--------------------

The image contains Apache, the CGI and the `Algorithms` binary (Ubuntu 26.04, OpenMPI 5, gSOAP 2.8):

```sh
docker build -f docker/Dockerfile --build-context algorithms=../DAMIS -t damis-service .
docker run -d -p 127.0.0.1:8087:80 \
  -e SERVICE_URL=http://localhost:8087/cgi-bin/DamisService.cgi \
  -e DATA_URL=http://localhost:8087/DamisData/ damis-service
```

In the DAMIS-GUI repository `docker compose up -d` builds and starts it as the `damis-service` container.

| Variable | Default | Meaning |
|---|---|---|
| `SERVICE_URL` | `http://localhost/cgi-bin/DamisService.cgi` | address written into the WSDL (`soap:address`), must be reachable by the client |
| `DATA_URL` | `http://localhost/DamisData/` | public URL of the result files (`Y` = `DATA_URL` + `_output_*.arff`) |
| `MPI_PROCESSES` | number of CPUs | maximum `p` (processes per algorithm run); larger requests are rejected |
| `MPI_OPTIONS` | `--oversubscribe --bind-to none` | extra `mpirun` options |
| `MAX_RUN_TIME` | `3300` | seconds after which an algorithm run is killed (Apache `Timeout` is 3600) |
| `KEEP_FILES_SECONDS` | `172800` | data files older than this are deleted on the next request |

Volumes: `/var/lib/damis/data` (inputs/results, failed runs are copied to `err/`), `/var/log/damis`
(`damis-service.log`). **Do not expose the service publicly**: it downloads any URL given in `X`.

Build without Docker
--------------------

```sh
sudo apt-get install build-essential gsoap libgsoap-dev libcurl4-openssl-dev   # Ubuntu 26.04 / Debian 13
make -j"$(nproc)"                      # gSOAP stubs are generated from wsdl/damis.wsdl into build/gen
sudo make install PREFIX=/usr/lib/cgi-bin
```

The CGI reads `settings.csv` from its own directory (`key value` per line, `%` or `#` comments):
`wsdlPath`, `logingConfFilePath`, `pathToAlgorithmsExe`, `pathToMPIExecutable`, `mpiOptions`,
`noOfProcessors`, `maxRunTime`, `localDataFileSavePath`, `workingDirMPI`, `httpDataFileSavePath`,
`dataFileExtension`, `lenghtOfTmpFileName`, `keepFilesInDirectory` - see `docker/entrypoint.sh`.

Changes in 2026
---------------

- **Every SOAP response was invalid for PHP** ("looks like we got no XML document"): `getcgivars()`
  printed its own `Content-Type` header and an error for the `text/xml` POST, which ended up after the
  envelope. `?wsdl` is now detected from `QUERY_STRING` only.
- **Errors produced two SOAP envelopes** (`sendError()` sent the fault and `serve()` sent it again).
- **Algorithms run locally** with `mpirun` (fork/exec, own process group, `MAX_RUN_TIME` limit, output
  captured, fault with the last output lines on failure). The SLURM (`sbatch`) and Grid Engine (`qsub`)
  cluster submission was removed; with the January 2026 configuration (`runDestination 0`) no algorithm
  was run at all and a non-existing result file was returned.
- Unique temporary file names (`std::random_device`; requests in the same second used to share names),
  `calcTime` is wall time (was CPU time of the CGI), `algorithmError` no longer uninitialised.
- **WSDL corrected to the implemented interface** (MLP `qty`/`kFoldValidation`, DF, SOMMDS/KMEANS
  responses, no C45) and gSOAP stubs regenerated at build time with the current gSOAP (the bundled 2.8.16
  from 2013 predates CVE-2017-9765); UTF-8 strings are passed through (`SOAP_C_UTFSTRING`).
- Bugs: PCA `projType` validation swapped (percentage mode was always rejected); FILTERDATA ignored the
  mean (`|x| > zσ` instead of `|x − mean| > zσ`) and used a 1-based `attrIndex` although the GUI sends
  0-based indices; SPLITDATA reshuffling always gave the same order (`RAND_MAX+1` overflow); a malformed
  ARFF file crashed the CGI (HTTP 500); undefined behaviour in `strToInt`/`strToDouble`; logging could go
  to stdout (= the SOAP response) before `logging.conf` was loaded; `HOME` is set for `mpirun` (Apache
  starts CGIs without it).
- Removed: vendored curl/OpenSSL 0.9.8/Windows libraries and executables, stale Windows sources
  (`DamisService.cpp`, `InitDamisServiceFile.cpp`), `CGIvars/`, the committed gSOAP code.
