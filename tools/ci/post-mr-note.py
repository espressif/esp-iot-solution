#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import sys, os, urllib3, argparse
# Silence the irritating insecure warnings.
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Add the location of python-gitlab to the path so we can import it
repo_top = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '..'))
python_gitlab_dir = os.path.join(repo_top, 'external/python-gitlab')
sys.path.append(python_gitlab_dir)
import gitlab


def getArgs():
    """
    Parse the command line arguments.
    Usage: python post-mr-note.py [PROJECT-TOKEN] [PROJECT] [MR-IID]
    Sample usage: python post-mr-note.py [PROJECT-TOKEN] espressif/esp-hardware-design-guidelines 7
    """
    parser = argparse.ArgumentParser(description='Post note to existing MR.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('authkey', type=str, help='Project or personal access token for authentication with GitLab. Create one at https://gitlab.com/profile/personal_access_tokens')
    parser.add_argument('project', type=str, help='Path to GitLab project in the form <namespace>/<project>')
    parser.add_argument('mr_iid', type=str, help='Merge request number to post the message')
    parser.add_argument('--url', help='Gitlab URL.', default='https://gitlab.espressif.cn:6688')
    return parser, parser.parse_args()


class PythonGitlabNotes():
    """
    Collect links to artifacts saved by other jobs in logs folder.
    Post the links as a note to an existing merge request.
    """
    def __init__(self, url, authkey, project, mr_iid):
        # Parse command line arguments
        self.url = url
        self.url_api = url + '/api/v4'
        self.authkey = authkey
        self.project_name = project
        self.mr_iid = mr_iid
        # Create python-gitlab server instance
        server = gitlab.Gitlab(self.url, myargs.authkey, api_version=4, ssl_verify=False)
        # Get an instance of the project and store it off
        self.project = server.projects.get(self.project_name)


    def collect_data(self):
        series_links = {}
        with open('logs/doc-url.txt', 'r') as file:
            for line in file:
                if line.startswith('[document preview]'):
                    tokens = line.split(']')
                    desc_url = tokens[2]
                    lang_chip_info = tokens[1]
                    lang_chip = lang_chip_info.strip(']').strip('[').split('_')
                    if len(lang_chip) == 1:
                        language = 'en'
                        chip_series = 'reserve'  # Reserve for the future
                    if len(lang_chip) == 2:
                        language = 'zh_CN'
                        chip_series = 'reserve'  # Reserve for the future

                    if chip_series in series_links:
                        language_links = series_links[chip_series]
                        language_links[language] = desc_url
                    else:
                        language_links = {language: desc_url}
                    series_links[chip_series] = language_links
        self.series_links = series_links
        print(series_links)  # Debugging line

    def prepare_note(self):
        """
        Prepare a note with links to be posted in .md format.
        """
        note = 'Documentation preview:\n\n'
        for chip_series, language_links in self.series_links.items():
            product_name = chip_series.upper()  # Default value
            if chip_series == 'reserve':  # Reserve for the future
                product_name = 'ESP-IOT-SOLUTION'
            elif chip_series == 'reserve2':  # Reserve for the future
                product_name = 'ESP-IOT-SOLUTION'

            # note += f"- {product_name} "
            if 'zh_CN' in language_links:
                note += f"[ESP-IOT-SOLUTION CN]({language_links['zh_CN']})/"
            else:
                note += 'ESP-IOT-SOLUTION CN'
            if 'en' in language_links:
                note += f"[ESP-IOT-SOLUTION EN]({language_links['en']})"
            else:
                note += 'ESP-IOT-SOLUTION EN'
            note += '\n'
        self.note = note
        print(note)  # Debugging line

    def post_note(self):
        """
        Post the note to the merge request.
        """
        # Get the MR
        mr = self.project.mergerequests.get(self.mr_iid)
        # Post the note to the MR
        mr.notes.create({'body': self.note})

    def run(self):
        self.collect_data()
        self.prepare_note()
        self.post_note()


if __name__ == '__main__':
    myParser, myargs = getArgs()
    sys.exit(PythonGitlabNotes(url=myargs.url, authkey=myargs.authkey,
        project=myargs.project, mr_iid=myargs.mr_iid).run())
