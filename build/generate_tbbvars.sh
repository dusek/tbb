#!/bin/bash
#
# Copyright 2005-2008 Intel Corporation.  All Rights Reserved.
#
# This file is part of Threading Building Blocks.
#
# Threading Building Blocks is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# Threading Building Blocks is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Threading Building Blocks; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As a special exception, you may use this file as part of a free software
# library without restriction.  Specifically, if other files instantiate
# templates or use macros or inline functions from this file, or you compile
# this file and link it with other files to produce an executable, this
# file does not by itself cause the resulting executable to be covered by
# the GNU General Public License.  This exception does not however
# invalidate any other reasons why the executable file might be covered by
# the GNU General Public License.

# Script used to generate tbbvars.[c]sh scripts
bin_dir="$PWD"  # 
cd "$tbb_root"  # keep this comments here
tbb_root="$PWD" # to make it unsensible
cd "$bin_dir"   # to EOL encoding
[ "`uname`" = "Darwin" ] && dll_path="DYLD_LIBRARY_PATH" || dll_path="LD_LIBRARY_PATH" #
custom_exp="$CXXFLAGS" #
if [ -z "$TBB_CUSTOM_VARS_SH" ]; then #
custom_exp_sh="" #
custom_var_sh="" #
else #
custom_exp_sh="$TBB_CUSTOM_VARS_SH" #
custom_name_sh=`echo "$TBB_CUSTOM_VARS_SH" | sed 's/=.*$//'` # get variable name from assignment
custom_var_sh="export ${custom_name_sh}" #  string must be complete command or empty
fi #
if [ -z "$TBB_CUSTOM_VARS_CSH" ]; then #
custom_exp_csh="" #
else #
custom_exp_csh="setenv $TBB_CUSTOM_VARS_CSH" #
fi #
if [ -z "$1" ]; then # custom tbb_build_dir, can't make with TBB_INSTALL_DIR
[ -f ./tbbvars.sh ] || cat >./tbbvars.sh <<EOF
#!/bin/bash
tbb_root="${tbb_root}" #
tbb_bin="${bin_dir}" #
if [ -z "\$CPATH" ]; then #
    CPATH="\${tbb_root}/include" #
    export CPATH #    
else #
    CPATH="\${tbb_root}/include:\$CPATH" #
    export CPATH #    
fi #
if [ -z "\$LIBRARY_PATH" ]; then #
    LIBRARY_PATH="\${tbb_bin}" #
    export LIBRARY_PATH #    
else #
    LIBRARY_PATH="\${tbb_bin}:\$LIBRARY_PATH" #
    export LIBRARY_PATH #    
fi #
if [ -z "\$${dll_path}" ]; then #
    ${dll_path}="\${tbb_bin}" #
    export ${dll_path} #    
else #
    ${dll_path}="\${tbb_bin}:\$${dll_path}" #
    export ${dll_path} #    
fi #
${custom_exp_sh} #
${custom_var_sh} #
EOF
[ -f ./tbbvars.csh ] || cat >./tbbvars.csh <<EOF
#!/bin/csh
setenv tbb_root "${tbb_root}" #
setenv tbb_bin "${bin_dir}" #
if (! \$?CPATH) then #
    setenv CPATH "\${tbb_root}/include" #
else #
    setenv CPATH "\${tbb_root}/include:\$CPATH" #
endif #
if (! \$?LIBRARY_PATH) then #
    setenv LIBRARY_PATH "\${tbb_bin}" #
else #
    setenv LIBRARY_PATH "\${tbb_bin}:\$LIBRARY_PATH" #
endif #
if (! \$?${dll_path}) then #
    setenv ${dll_path} "\${tbb_bin}" #
else #
    setenv ${dll_path} "\${tbb_bin}:\$${dll_path}" #
endif #
${custom_exp_csh} #
EOF
else # make with TBB_INSTALL_DIR
[ -f ./tbbvars.sh ] || cat >./tbbvars.sh <<EOF
#!/bin/bash
TBB21_INSTALL_DIR="${tbb_root}" #
export TBB21_INSTALL_DIR #
tbb_bin="\${TBB21_INSTALL_DIR}/build/$1" #
if [ -z "\$CPATH" ]; then #
    CPATH="\${TBB21_INSTALL_DIR}/include" #
    export CPATH #    
else #
    CPATH="\${TBB21_INSTALL_DIR}/include:\$CPATH" #
    export CPATH #    
fi #
if [ -z "\$LIBRARY_PATH" ]; then #
    LIBRARY_PATH="\${tbb_bin}" #
    export LIBRARY_PATH #    
else #
    LIBRARY_PATH="\${tbb_bin}:\$LIBRARY_PATH" #
    export LIBRARY_PATH #    
fi #
if [ -z "\$${dll_path}" ]; then #
    ${dll_path}="\${tbb_bin}" #
    export ${dll_path} #    
else #
    ${dll_path}="\${tbb_bin}:\$${dll_path}" #
    export ${dll_path} #    
fi #
${custom_exp_sh} #
${custom_var_sh} #
EOF
[ -f ./tbbvars.csh ] || cat >./tbbvars.csh <<EOF
#!/bin/csh
setenv TBB21_INSTALL_DIR "${tbb_root}" #
setenv tbb_bin "\${TBB21_INSTALL_DIR}/build/$1" #
if (! \$?CPATH) then #
    setenv CPATH "\${TBB21_INSTALL_DIR}/include" #
else #
    setenv CPATH "\${TBB21_INSTALL_DIR}/include:\$CPATH" #
endif #
if (! \$?LIBRARY_PATH) then #
    setenv LIBRARY_PATH "\${tbb_bin}" #
else #
    setenv LIBRARY_PATH "\${tbb_bin}:\$LIBRARY_PATH" #
endif #
if (! \$?${dll_path}) then #
    setenv ${dll_path} "\${tbb_bin}" #
else #
    setenv ${dll_path} "\${tbb_bin}:\$${dll_path}" #
endif #
${custom_exp_csh} #
EOF
fi #
