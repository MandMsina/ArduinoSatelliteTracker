from flask import Flask, request
from flask_cors import CORS, cross_origin
import requests
from datetime import datetime
import json
import os

HOST = 'https://www.space-track.org'
PAYLOAD = {'identity': "YOURLOGIN",
           'password': "YOUR PASSWORD"}

app = Flask(__name__)
cors = CORS(app)
app.config['CORS_HEADERS'] = 'Content-Type'
session = None
login_response = None
last_login_time = None

last_cashed_pager = None


class Pager:
    def __init__(self, requested_name, list_of_satellites, n=10):
        self.requested_name = requested_name
        self.list_of_pages = list()
        for i in range(0, len(list_of_satellites), n):
            self.list_of_pages.append(list_of_satellites[i:i + n])

    def get_page(self, page_number):
        if page_number > len(self.list_of_pages):
            return []
        return self.list_of_pages[page_number - 1]

    def get_name(self):
        return self.requested_name

    def size(self):
        return len(self.list_of_pages)


def create_session():
    global session, login_response, last_login_time
    session = requests.Session()
    login_response = session.post(HOST + '/ajaxauth/login', data=PAYLOAD)
    last_login_time = datetime.now()


def validate_sat_info(list_of_satellites):
    result = list()
    first_satellite = None
    for satellite in list_of_satellites:
        if int(satellite['DECAYED']) == 0:
            if first_satellite is None:
                first_satellite = dict()
                first_satellite['name'] = satellite['OBJECT_NAME']
                first_satellite['tle1'] = satellite['TLE_LINE1']
                first_satellite['tle2'] = satellite['TLE_LINE2']

            validated_satellite = dict()
            validated_satellite['norad_id'] = satellite['NORAD_CAT_ID']
            validated_satellite['object_name'] = satellite['OBJECT_NAME']
            result.append(validated_satellite)
    if len(result) == 1:
        return [first_satellite]
    return result


@app.route('/get/tle')
@cross_origin()
def hello():
    global session, login_response, last_login_time, last_cashed_pager
    satname = request.args.get('satname', default=None, type=str)
    norad_id = request.args.get('norad_id', default=None, type=int)
    page_number = request.args.get('page', default=1, type=int)

    if satname is not None and \
            last_cashed_pager is not None and \
            last_cashed_pager.get_name() == satname:
        page = last_cashed_pager.get_page(page_number)
        return json.dumps({
                'empty_result': len(page) == 0,
                'is_only_one_result': False,
                'list_of_variants': page,
                'page_number': page_number,
                'number_of_pages': last_cashed_pager.size(),
            }), 200

    if (satname is not None and norad_id is not None) or \
       (satname is None and norad_id is None):
        return json.dumps({
            'empty_result': True,
            'reason': 'One of ["satname", "norad_id"] is required'
            }), 400
    if last_login_time is None or \
       (datetime.now() - last_login_time).seconds > 1800:
        create_session()

    address = HOST + '/basicspacedata/query/class/tle_latest'
    if satname is not None:
        address += f'/OBJECT_NAME/~~{satname}/format/json/ORDINAL/1'
    if norad_id is not None:
        address += f'/NORAD_CAT_ID/{norad_id}/format/json/ORDINAL/1'

    response = session.get(address)
    if response.status_code != 200:
        last_login_time = None
        return json.dumps({
            'empty_result': True,
            'reason': 'space-track return status code ' + \
                       str(response.status_code)
        }), 200
    else:
        result = response.json()
        validated_result = validate_sat_info(result)

        if len(validated_result) == 1:
            return json.dumps({
                'empty_result': False,
                'is_only_one_result': True,
                'satellite_identity': validated_result[0],
            }), 200

        elif len(validated_result) > 1:
            last_cashed_pager = Pager(
                requested_name=satname,
                list_of_satellites=validated_result
            )
            return json.dumps({
                'empty_result': False,
                'is_only_one_result': False,
                'page_number': page_number,
                'list_of_variants': last_cashed_pager.get_page(page_number),
                'number_of_pages': last_cashed_pager.size(),
            }), 200

        else:
            reason = 'there is no element with this identity'
            if len(result) != 0:
                reason = ('finded satellite(s) '
                          ' is no longer in orbit')
            return json.dumps({
                'empty_result': True,
                'reason': reason
            }), 200


app.run(host='0.0.0.0', port=os.getenv('PORT'))
