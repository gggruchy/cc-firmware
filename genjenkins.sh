#!/bin/bash

GITHUB_SHA="$(git rev-parse --verify HEAD)"
GITHUB_REF_NAME="$($(dirname $0)/getversion.sh)"
JENKINS_CUSTOMER="OpenCentauri"
JENKINS_BOARD="e100"

cat jenkins_tmpl.h | sed -r -e "s|##JENKINS_COMMIT_ID##|${GITHUB_SHA::7}|" -e "s|##JENKINS_VERSION##|${GITHUB_REF_NAME}|" -e "s|##JENKINS_CUSTOMER##|${JENKINS_CUSTOMER}|" -e "s|##JENKINS_BOARD##|${JENKINS_BOARD}|" > jenkins.h

