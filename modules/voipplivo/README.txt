== Introduction ==

Plivo Framework (http://plivo.com/opensource) is a Communications Framework to rapidly build voice based apps, to make or receive calls, using your existing web development skills and 
your existing infrastructure. To use it you must install Plivo Framework at your own server.

Plivo Cloud (http://plivo.com/) is API platform to build Voice & SMS Applications. There is no need for server installation.

Enable voipplivoframework.module if you wish to use Plivo Framework, or voipplivocloud.module if you wish to use Plivo Cloud.

******************************************************************************************************************************
1. Plivo Framework:
== Requirements==

In order to install the voipplivoframework.module, you will need:

1. Configure and run Plivo Framwork and FreeSWITCH(http://docs.plivo.org/get-started/)

2. The VoIP Drupal module (http://drupal.org/project/voipdrupal)

3. The PHP Curl extension in your system. For Debian systems, just run
  $ sudo apt-get install php5-curl
  $ sudo /etc/init.d/apache2 restart 


== Installation ==

Installing voipplivoframework.module is simple.  It requires a few configuration steps on your Drupal site 
to let it know how to reach your Plivo server. It also requires a few settings in your Plivo configuration to 
make sure it knows which Drupal site to use.


Plivo configuration:

1. Go to the /pathtoplivoinstall/etc/plivo and open default.conf in editor

2. Change this values:

  - DEFAULT_HTTP_METHOD = POST

  - DEFAULT_ANSWER_URL = http://mysite.com/voip/plivo/callhandler/ (for clean URLs) or http://mysite.com/?q=voip/plivo/callhandler/

  - EXTRA_FS_VARS = variable_duration

  - AUTH_ID = enter any value, this is your authentication id

  - AUTH_TOKEN = enter any value, this is your authentication token


3. Save the file and restart Plivo


Drupal configuration:

1. Install and enable voipplivoframework.module

2. Set Plivo Framework as the default voip server

  - Go to admin/voip/servers

  - Click on Plivo Framework "configure" link

  - Fill in the fields "Account SID" and "Auth Token" with your Plivo "AUTH_ID" and "AUTH_TOKEN" values, respectively (see "Plivo configuration" above)
  
  - If your Plivo is on a different server than Drupal, change the value of "Plivo REST API Url" to the new server's URL
  
  - Optionally click "Plivo Outbound Call Parameters", to set up advanced options as per your needs
  
  - Press "Save". That will take you back to admin/voip/servers

  - Select the 'Plivo Framework' option

  - Press the 'Set default voip server' button


3. Enable users to make outgoing (outbound) calls from your site

  - Go to admin/user/permissions

  - Find the "voip module" permissions

  - Enable the "make outbound calls" permission for the desired roles

  - Press the "save permissions" button   

  
== Try it out ==    

Now you should be able to call your VoIP Drupal site on  Plivo default number (1000@yourserverip:5080). Enjoy!


******************************************************************************************************************************
2. Plivo Cloud:
== Requirements ==

In order to install the voipplivocloud.module, you will need:

1. A Plivo Cloud account

2. The PHP Curl extension in your system. For Debian systems, just run
  $ sudo apt-get install php5-curl
  $ sudo /etc/init.d/apache2 restart 


== Installation ==

Installing voipplivocloud.module is very simple.  It requires a few configuration steps on your Drupal site to let it 
know how to reach your Plivo account It also requires a few settings in your Plivo account to make sure it knows which Drupal site to use.

Drupal configuration:

1. Install and enable voipplivocloud.module

2. Set Plivo Cloud as the default voip server
  - Go to admin/voip/servers

  - Click on Plivo Cloud "configure" link

  - Fill in the fields with the "Auth ID" and "Auth Token" associated with your Plivo account. 
    Both of those values can be found in your Plivo account's "Dashboard" (https://www.plivo.com/dashboard/)

  - Go back to admin/voip/servers

  - Select the 'Plivo Cloud' option

  - Press the 'set default voip server' button


Plivo Cloud configuration:

1. Login into your Plivo Cloud account

2. Create new application from "Applications" section
   Set the URLs associated with your site
   
  - Fill the "Answer url" field with
    http://mysite.com/voip/plivocloud/callhandler/process_inbound_calls (for clean URLs)
    or http://mysite.com/?q=voip/plivocloud/callhandler/process_inbound_calls
    
  - Fill the "Message url" field with
    http://mysite.com/voip/plivocloud/callhandler/process_inbound_text (for clean URLs)
    or http://mysite.com/?q=voip/plivocloud/callhandler/process_inbound_text

  - Fill the "Hangup url" field with
    http://mysite.com/voip/plivocloud/callhandler/process_hangup (for clean URLs)
    or http://mysite.com/?q=voip/plivocloud/callhandler/process_hangup

  - Make sure Answer method, Message method and Fallback method are set to use "POST"

  - Press the "Save" button

  - Enjoy!
  
3. In the "Numbers" section of the account, click on the "Rent Phone Number" link and choose a number for your application. 

4. Enter this number at VoIP Drupal default call configuration at admin/voip/call/settings

== About ==

The VoIP Plivo module has been originally developed by Leo Burd, Tamer Zoubi and Blair MacNeil under the sponsorship of the MIT Center for Civic Media (http://civic.mit.edu).
