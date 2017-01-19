Write-Host "NFDecoder build process starting..."

$ErrorActionPreference = "Stop"

try
{
	# Get python version
	$python_version = python --version
	Write-Host $python_version

	# Start virtualenv
	$virtualenv_vulcan_output = python tools/vulcan/bin/vulcan.py -v -f tools/virtualenv.vulcan -p virtualenv-15.1.0
	$virtualenv_bin = Join-Path $virtualenv_vulcan_output /virtualenv-15.1.0/virtualenv.py
	python $virtualenv_bin nfdecoder_env

	& ./nfdecoder_env/Scripts/activate.bat

	# Install Python Packages
	& nfdecoder_env/Scripts/pip.exe install pyyaml
	& nfdecoder_env/Scripts/pip.exe install flake8
	& nfdecoder_env/Scripts/pip.exe install cmakelint
	& nfdecoder_env/Scripts/pip.exe install requests
	& nfdecoder_env/Scripts/pip.exe install pysoundfile
	& nfdecoder_env/Scripts/pip.exe install numpy

	# Install gyp
	cd tools/gyp
	& ../../nfdecoder_env/Scripts/python.exe setup.py install
	cd ../..

	& nfdecoder_env/Scripts/python.exe ci/androidwindows.py
	if($LASTEXITCODE -ne 0)
	{
		exit $LASTEXITCODE
	}

	& ./nfdecoder_env/Scripts/deactivate.bat
}
catch
{
	echo $_.Exception|format-list -force
	exit 1
}
