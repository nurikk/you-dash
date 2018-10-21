import requests
params = {
    "currency": "USD",
    "ids": "channel==MINE",
    "endDate": "2018-11-01",
    "startDate": "2018-10-01",
    "metrics": "views,comments,likes,dislikes,estimatedMinutesWatched,averageViewDuration",
    "dimensions": "30DayTotals",
    "fields": "columnHeaders,errors,kind,rows"

}
TOKEN = 'ya29.Gls9BkDr31cvrwmj9ARrWWyO5eTH2dZthumfM2xN5VSOCYZ_r9sb7EDGu2SVCB4AnUYsxFrwwarQto_6cupaheyzi6fP6KqfasaQKyAoOeBK9Gv5rBgPj7hvUmO4'
headers = {
    "Authorization": f'Bearer {TOKEN}'
}
url = 'https://youtubeanalytics.googleapis.com/v2/reports'

resp = requests.get(url, params=params, headers=headers)
print(resp.text)
# import pdb; pdb.set_trace()