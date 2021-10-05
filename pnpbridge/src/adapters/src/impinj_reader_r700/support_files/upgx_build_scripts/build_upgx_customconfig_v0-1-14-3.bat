echo Building UPGX with custom config.json file... 
docker run -d -t --name pnpbridge_r700_cap_build kwennerimpinj/impinj_r700_azure_pnpbridge_buildcap:v0.1.14.3
docker cp .\config.json pnpbridge_r700_cap_build:/home/pnpbridge/impinj_adapter_r700/support_files/
docker exec --workdir /home/pnpbridge/impinj_adapter_r700/cap pnpbridge_r700_cap_build make
if exist export\ (RMDIR /Q /S export)

MKDIR export
docker cp pnpbridge_r700_cap_build:/home/pnpbridge/impinj_adapter_r700/cap/build/azure_pnpbridge_impinj_r700_v0-1-14-3.upgx ./export/azure_pnpbridge_impinj_r700_v0-1-14-3_custconfig.upgx
docker stop pnpbridge_r700_cap_build
docker rm pnpbridge_r700_cap_build

echo UPGX Output: %cd%\export\azure_pnpbridge_impinj_r700_v0-1-14-3_custconfig.upgx