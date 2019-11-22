#!/bin/bash
#
curl --fail --silent --show-error -X POST --header 'Content-Type: application/json' --header 'Accept: application/json' -d '{"username":"climb@smartcommunitylab.it", "password":"RZnOXVTWMY5sDS"}' 'https://iot.smartcommunitylab.it/api/auth/login'

