#!/bin/bash

curl --header "Content-Type: application/json" \
  --request POST \
  --data '{"film":"/home/carl/DCP/big"}' \
  http://localhost:8000/api/v1/jobs/add
