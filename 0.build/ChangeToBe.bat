@echo off
setlocal EnableDelayedExpansion
set "PROJECT_PATH=D:\Projects\Projects_Qualcomm\HECA_11BCBe\headset\workspace\QCC3044-AA_DEV-BRD-R2-AA"
copy /Y %PROJECT_PATH%\headset_bkup_11Be.x2p %PROJECT_PATH%\headset.x2p >nul
copy /Y %PROJECT_PATH%\filesystems\ps_cfg_bkup_11Be.x2p %PROJECT_PATH%\filesystems\ps_cfg.x2p >nul
copy /Y %PROJECT_PATH%\filesystems\ro_fs_11BCBe.x2p %PROJECT_PATH%\filesystems\ro_fs.x2p >nul
copy /Y %PROJECT_PATH%\filesystems\subsys7_config5_11Be.htf %PROJECT_PATH%\filesystems\subsys7_config5.htf >nul
copy /Y %PROJECT_PATH%\filesystems\subsys1_config2_11Be.htf %PROJECT_PATH%\filesystems\subsys1_config2.htf >nul
endlocal
echo 11Be mode!!
timeout /t 2