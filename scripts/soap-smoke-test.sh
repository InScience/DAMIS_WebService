#!/usr/bin/env bash
# SOAP smoke test of a running DamisService (e.g. the docker/Dockerfile image), with curl only:
#
#   scripts/soap-smoke-test.sh SERVICE_URL DATA_URL
#
#   SERVICE_URL  the CGI, e.g. http://localhost:8087/cgi-bin/DamisService.cgi
#   DATA_URL     a directory URL the service can download iris.arff from (150 objects, 4 numeric attributes,
#                as arff_files/iris.arff of the DAMIS repository)
#
# The result files are downloaded from the URLs the service returns, so its DATA_URL setting must be
# reachable from where this script runs. Exit code 1 if any check fails.
set -u
SERVICE=${1:?SERVICE_URL}
DATA=${2:?DATA_URL}
X="${DATA%/}/iris.arff"
fail=0

# call OPERATION PARAMETERS-XML -> SOAP response on stdout
call() {
    curl -s --max-time 600 -H 'Content-Type: text/xml; charset=utf-8' -H 'SOAPAction: ""' --data-binary @- "$SERVICE" <<XML
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="DAMIS" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" SOAP-ENV:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><SOAP-ENV:Body><ns1:$1>$2</ns1:$1></SOAP-ENV:Body></SOAP-ENV:Envelope>
XML
}
p() { echo "<$1 xsi:type=\"xsd:$2\">$3</$1>"; } # name type value
element() { sed -nE "s|.*<$1>([^<]*)</$1>.*|\1|p"; }
# data rows of an ARFF file at a URL (lines that are not empty, @ declarations or % comments)
rows() { curl -s --fail --max-time 60 "$1" | grep -cE '^[[:space:]]*[^@%[:space:]]' || true; }

# expect_result LABEL RESPONSE ELEMENT=ROWS...
expect_result() {
    local label=$1 response=$2 spec url n ok=1 out=""
    shift 2
    for spec in "$@"; do
        url=$(element "${spec%%=*}" <<< "$response")
        n=$([ -n "$url" ] && rows "$url" || echo none)
        out+=" ${spec%%=*}=$n"
        [ "$n" = "${spec#*=}" ] || ok=0
    done
    if [ $ok = 1 ]; then echo "  ok   $label:$out"; else echo "  FAIL $label:$out (expected $*)"; echo "       $(head -c 600 <<< "$response")"; fail=1; fi
}

# expect_fault LABEL RESPONSE DETAIL-SUBSTRING
expect_fault() {
    local detail
    detail=$(element detail <<< "$2")
    if [[ "$detail" == *"$3"* ]]; then echo "  ok   $1: fault \"$detail\""; else echo "  FAIL $1: expected a fault with \"$3\""; echo "       $(head -c 600 <<< "$2")"; fail=1; fi
}

echo "== WSDL"
if curl -s --fail "$SERVICE?wsdl" | grep -q '<WSDL:operation name="SMACOFMDS">'; then echo "  ok   $SERVICE?wsdl"; else echo "  FAIL $SERVICE?wsdl"; fail=1; fi

echo "== operations"
expect_result "PCA d=2" "$(call PCA "$(p X string "$X")$(p projType boolean false)$(p d double 2)$(p maxCalcTime int 60)")" Y=150
expect_result "SMACOFMDS on 2 MPI processes" \
    "$(call SMACOFMDS "$(p X string "$X")$(p d int 2)$(p maxIteration int 100)$(p eps double 0.0001)$(p zeidel boolean false)$(p p int 2)$(p maxCalcTime int 60)")" Y=150
expect_result "KMEANS" "$(call KMEANS "$(p X string "$X")$(p kMax int 3)$(p maxIteration int 100)$(p maxCalcTime int 60)")" Y=150
expect_result "STATPRIMITIVES" "$(call STATPRIMITIVES "$(p X string "$X")$(p maxCalcTime int 60)")" Y=4
expect_result "SPLITDATA 70/30" \
    "$(call SPLITDATA "$(p X string "$X")$(p reshufleObjects boolean true)$(p firstSubsetPerc double 70)$(p secondSubsetPerc double 30)$(p maxCalcTime int 60)")" Y=105 Yalt=45

echo "== faults"
expect_fault "PCA d=0" "$(call PCA "$(p X string "$X")$(p projType boolean false)$(p d double 0)$(p maxCalcTime int 60)")" "d must be in interval"
expect_fault "missing input file" "$(call PCA "$(p X string "${DATA%/}/missing.arff")$(p projType boolean false)$(p d double 2)$(p maxCalcTime int 60)")" "was not successful"

[ $fail = 0 ] && echo "RESULT: all checks passed" || echo "RESULT: some checks FAILED"
exit $fail
