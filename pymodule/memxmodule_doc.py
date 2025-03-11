import numpy as np
class mxa:
    # Dummpy class of memxmodule for doxygen
    """
    MemryX driver.

    The interface to the MemryX driver. This class wraps MemryX driver C API to Python.
    """

    def lock(self, group_id:int):
        """
        Acquires mutex lock from driver. To be noticed, this function will not actually lock any operation but only acquire the unique mutex within driver. In multi-threads programming, user can use this lock to prevent driver access conflict. Also, this function will block until mutex is acquired.

        Parameters
        ----------
            group_id : int
                MPU device group ID
        """
        return

    def unlock(self, group_id:int):
        """
        Releases the mutex lock acquired from driver. Be careful and not to unexpectedly release mutex which is not locked by current process (thread).

        Parameters
        ----------
            group_id : int
                MPU device group ID
        """
        return

    def open(self, model_id:int, group_id:int, chip_gen:int):
        """
        Open device with interface auto-detected. Currently only one device connected to host is supported within this version implementation. Even in case chips are cascaded to compose data flow, there is only one device can be seen within host.

        Parameters
        ----------
            model_id : int
                Model ID

            group_id : int
                MPU device group ID

            chip_gen : int
                Target system
                Casade = 3
        """
        return

    def close(self, model_id:int):
        """
        Close current connected device and interface in use. This function returns always no error even if device is not opened.

        Parameters
        ----------
            model_id : int
                Model ID
        """
        return

    def download(self, model_id:int, file_path:str, model_idx:int, type:int):
        """
        Download weight memory and model to device based on given download type selection. If both weight memory and model download are required, weight memory download will be performed before model download. Also after model is downloaded, input and output feature map shape will be configured to driver automatically.

        Parameters
        ----------
            model_id : int
                Model ID

            file_path : string
                DFP file path

            model_idx : int
                Which rgcfg to use (used in model swapping). Set to 0 if only 1 model.

            type : int
                Target model type (any of the following)

                * :code:`mxa.download_type_wtmem` / :code:`1`
                * :code:`mxa.download_type_model` / :code:`2`
                * :code:`mxa.download_type_wtmem_and_model` / :code:`3`
        """
        return

    def download_model_config():
        """
        Download model config to device.

        Parameters
        ----------
            model_id : int
                Model id of MPU.

            file_path :string
                Path to dfp file.

            model_idx : int
                Model index of download model.
        """
        return

    def update_firmware():
        """
        Download firmware binary to device.

        Parameters
        ----------
            model_id : int
                Model id of MPU.

            file_path :string
                Path to firmware binary file.
        """
        return

    def download_model_wtmem():
        """
        Download model wtmem to device.

        Parameters
        ----------
            model_id : int
                Model id of MPU.

            file_path :string
                Path to dfp file.
        """
        return

    def set_stream_enable(self, model_id:int, wait:int):
        """
        Enable all input and output data flows of this MPU context to interface driver.

        Parameters
        ----------
            model_id : int
                Model ID

            wait : bool
                Wait until state changed or not
        """
        return

    def set_stream_disable(self, model_id:int, wait:int):
        """
        Disable all input and output data flows of this MPU context to interface driver.

        Parameters
        ----------
            model_id : int
                Model ID

            wait : bool
                Wait until state changed or not
        """
        return

    def set_worker_number():
        return

    def stream_ifmap(self, model_id:int, flow_id:int, ifmap:np.ndarray, timeout:int=0):
        """
        Data channel write frame to device.

        Parameters
        ----------
            model_id : int
                Model ID

            flow_id : int
                Input flow (port) ID

            ifmap : np.ndarray
                Input feature map (frame)

            timeout : int
                Milliseconds timeout, ‘0’ indicates infinite
        """
        return

    def stream_ofmap(self, model_id:int, flow_id:int, ofmap:np.ndarray, timeout:int=0):
        """
        Stream data out from device.

        Parameters
        ----------
            model_id : int
                Model ID

            flow_id : int
                Output flow (port) ID

            ofmap : np.ndarray
                Output feature map (frame).

            timeout : int
                Milliseconds timeout, ‘0’ indicates infinite
        """
        return
