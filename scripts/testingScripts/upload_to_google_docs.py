#!/usr/bin/python
#
# Upload an html file to google docs.
# Files are uploaded into a folder called "Nightly Tests".
#
# Usage: upload_to_google_docs.py file.html

import sys, os
import gdata.docs.service

# ACCOUNT_NAME should be something@corelab-research.org
ACCOUNT_NAME = ""
ACCOUNT_PASS = ""
FOLDER_NAME = "Nightly Tests"

if (len(sys.argv) != 2):
	print("Usage: %s file.html" % sys.argv[0])
	sys.exit(1)

pathname = sys.argv[1]
filename = os.path.basename(pathname)

# check that file exists and is a file

if (not os.path.isfile(pathname)):
	print("Cannot open file " + pathname)
	sys.exit(1)

# authenticate

gd_client = gdata.docs.service.DocsService()
gd_client.ClientLogin(ACCOUNT_NAME, ACCOUNT_PASS)

# get entry for folder, create if needed

q = gdata.docs.service.DocumentQuery(categories=['folder'], params={'showfolders': 'true'})
q['title'] = FOLDER_NAME
q['title-exact'] = 'true'
feed = gd_client.Query(q.ToUri())
if (len(feed.entry) > 0):
	folder = feed.entry.pop()
else:
	folder = gd_client.CreateFolder(FOLDER_NAME)

# upload the file

ms = gdata.MediaSource(file_path=pathname, content_type=gdata.docs.service.SUPPORTED_FILETYPES['HTML'])
entry = gd_client.Upload(ms, filename, folder_or_uri=folder)

# share document
# TODO

print "Uploaded", pathname
