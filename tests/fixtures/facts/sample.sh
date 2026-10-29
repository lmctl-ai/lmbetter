#!/bin/bash
source lib.sh
. "./other.sh"
source "$HOME/x.sh"
source "${DIR}/y.sh"
. $PFX/z.sh
NAME=world
export GREET="hi"
local_fn() { local v=1; echo "$NAME ${v} $1 ${#NAME}"; }
function f2 { return; }
local_fn arg
echo ${arr[0]} $((1+2))
