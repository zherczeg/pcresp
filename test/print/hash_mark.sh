#!/bin/bash

CWD=`pwd`
if [ -f "../../bin/pcresp" ]; then
    PATH="$CWD/../../bin":$PATH
else
  if [ -f "../bin/pcresp" ]; then
      PATH="$CWD/../bin":$PATH
  else
      echo "Cannot find pcresp build directory"
      exit
  fi
fi

echo "echo A | pcresp '(.)' -s '*print    AB    CD   <AB  CD>    < AB >     CD'"
echo A | pcresp '(.)' -s '*print    AB    CD   <AB  CD>    < AB >     CD'
echo

echo "A | pcresp '(*:MarK)(.)' -s '*print #1 #{1} #< #> ## #M'"
echo A | pcresp '(*:MarK)(.)' -s '*print #1 #{1} #< #> ## #M'
echo

echo "A | pcresp -d key value -d other_key other_value '(.|(.))' -s '*print #[key] #{1,key} #[other_key] #{2,key} [#[unknown_key]]'"
echo A | pcresp -d key value -d other_key other_value '(.|(.))' -s '*print #[key] #{1,key} #[other_key] #{2,key} [#[unknown_key]]'
echo

# Syntax error
echo "A | pcresp '.' -s '*print #[key] #a'"
echo A | pcresp '.' -s '*print #[key] #a'
echo
