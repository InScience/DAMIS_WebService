# DAMIS service image: DamisService.cgi (SOAP) under Apache + the MPI Algorithms binary.
#
# Build from this repository with the DAMIS algorithms repository as an additional context:
#   docker build -f docker/Dockerfile --build-context algorithms=../DAMIS -t damis-service .
# (docker compose: see DAMIS-GUI/docker-compose.yml, service "damis-service")

ARG UBUNTU_VERSION=26.04

FROM ubuntu:${UBUNTU_VERSION} AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential openmpi-bin libopenmpi-dev gsoap libgsoap-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# MPI algorithms (InScience/DAMIS)
COPY --from=algorithms . /src/DAMIS
RUN make -C /src/DAMIS clean \
    && make -C /src/DAMIS -j"$(nproc)" \
    && make -C /src/DAMIS install PREFIX=/opt/damis

# SOAP web service (this repository)
COPY . /src/DAMIS_WebService
RUN make -C /src/DAMIS_WebService clean \
    && make -C /src/DAMIS_WebService -j"$(nproc)" \
    && make -C /src/DAMIS_WebService install PREFIX=/usr/lib/cgi-bin

# fail the build if an algorithm does not work (mpirun refuses to run as root, use a user)
RUN useradd --create-home builder \
    && cp -r /src/DAMIS /tmp/damis-test && chown -R builder /tmp/damis-test \
    && su builder -c "/tmp/damis-test/scripts/smoke-test.sh /opt/damis/bin/Algorithms" \
    && rm -rf /tmp/damis-test

# name of the gSOAP runtime package matching libgsoap-dev (e.g. libgsoap-2.8.139)
RUN apt-cache depends libgsoap-dev | awk '/Depends: libgsoap-2/ { print $2 }' > /opt/damis/gsoap-runtime-package


FROM ubuntu:${UBUNTU_VERSION}
ENV DEBIAN_FRONTEND=noninteractive
COPY --from=build /opt/damis/gsoap-runtime-package /tmp/gsoap-runtime-package
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        apache2 openmpi-bin libcurl4t64 ca-certificates "$(cat /tmp/gsoap-runtime-package)" \
    && rm -rf /var/lib/apt/lists/* /tmp/gsoap-runtime-package

COPY --from=build /opt/damis/bin/Algorithms /opt/damis/bin/Algorithms
COPY --from=build /usr/lib/cgi-bin/DamisService.cgi /usr/lib/cgi-bin/DamisService.cgi
COPY wsdl/damis.wsdl /usr/share/damis/damis.wsdl
COPY docker/apache-damis.conf /etc/apache2/sites-available/damis.conf
COPY --chmod=0755 docker/entrypoint.sh /usr/local/bin/damis-entrypoint

RUN a2enmod -q cgid \
    && a2dissite -q 000-default \
    && a2disconf -q serve-cgi-bin \
    && a2ensite -q damis \
    && echo "ServerName damis-service" > /etc/apache2/conf-available/servername.conf \
    && a2enconf -q servername \
    && mkdir -p /etc/damis /var/lib/damis/data/err /var/log/damis \
    && chown -R www-data:www-data /var/lib/damis /var/log/damis

# Runtime configuration (see docker/entrypoint.sh and README.md)
ENV SERVICE_URL=http://localhost/cgi-bin/DamisService.cgi \
    DATA_URL=http://localhost/DamisData/ \
    MPI_PROCESSES="" \
    MPI_OPTIONS="--oversubscribe --bind-to none" \
    MAX_RUN_TIME=3300 \
    KEEP_FILES_SECONDS=172800

VOLUME ["/var/lib/damis/data", "/var/log/damis"]
EXPOSE 80

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD bash -c 'exec 3<>/dev/tcp/127.0.0.1/80 && printf "GET /cgi-bin/DamisService.cgi?wsdl HTTP/1.0\r\nHost: localhost\r\n\r\n" >&3 && grep -q "<definitions" <&3'

ENTRYPOINT ["damis-entrypoint"]
CMD ["apache2ctl", "-D", "FOREGROUND"]
