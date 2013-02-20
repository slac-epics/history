TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
DB += pv_history.template
DB += wave_proc.template
DB += ai_hist.substitutions
DB += ai_hist_pattern.db
DB += ai_hist_pattern.substitutions
DB += wave_proc_hist.db
DB += wave_proc_hist.substitutions

#----------------------------------------------------
# Create and install (or just install)
# Autosave request files, templates, substitutions like this
# REQ += autosave_history.req
REQ += history.tpl-req
REQ += history.sub-req
REQ += wave_proc.tpl-req

#----------------------------------------------------
# Create and install (or just install)
# Archive files, templates, substitutions like this
# ARCHIVE += archive_history.archive
ARCHIVE += history.tpl-arch
ARCHIVE += hist_LAST_N.tpl-arch
ARCHIVE += wave_LAST_N.tpl-arch
ARCHIVE += history.sub-arch

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

