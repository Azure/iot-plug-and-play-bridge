FROM kwennerimpinj/impinj_r700_azure_pnpbridge_buildcap:v0.1.14.2

WORKDIR .

ADD ./../../../../../../pnpbridge/src/adapters/src/impinj_reader_r700/cap /home/pnpbridge/impinj_adapter_r700/cap
ADD ./../../../../../../pnpbridge/cmake/pnpbridge_linux/src/pnpbridge/samples/console/pnpbridge_bin /home/pnpbridge/impinj_adapter_r700/cap

CMD bash
