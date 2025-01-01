<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2024-2025 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
require_once('openmediavault/globals.inc');

class OMVRpcServiceSafeCmd extends \OMV\Rpc\ServiceAbstract
{
    public function getName()
    {
        return 'SafeCmd';
    }

    public function initialize()
    {
        $this->registerMethod('getCmdList');
        $this->registerMethod('getCmd');
        $this->registerMethod('setCmd');
        $this->registerMethod('deleteCmd');
        $this->registerMethod('doCmd');

        $this->registerMethod('getUserCmdList');
        $this->registerMethod('doCmdUser');
    }

    public function getCmdList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->get('conf.service.safecmd.cmd');
        // Add additional command information
        $objectsAssoc = [];
	$repl = [',',':', PHP_EOL];
	$br = '<br />';
        foreach ($objects as $objectk => &$objectv) {
            $allow = str_replace($repl, $br, $objectv->get('allowlist'));
            $deny = str_replace($repl, $br, $objectv->get('denylist'));
            $objectv->add('allowlist2', 'string', $allow);
            $objectv->add('denylist2', 'string', $deny);
            $objectsAssoc[] = $objectv->getAssoc();
        }
        // Filter the result.
        return $this->applyFilter($objectsAssoc, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function getCmd($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        return $db->getAssoc('conf.service.safecmd.cmd', $params['uuid']);
    }

    public function setCmd($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.safecmd.setcmd');
	// replace delimiters with line feed
        $repl = [',', ':'];
	$allow = str_replace($repl, PHP_EOL, $params['allowlist']);
	$deny = str_replace($repl, PHP_EOL, $params['denylist']);
        // explode into an array
        $allows = explode(PHP_EOL, $allow);
        $denys = explode(PHP_EOL, $deny);
        // trim elements
        $allowt = array_map('trim', $allows);
        $denyt = array_map('trim', $denys);
	// remove empty elements
	$allowt = array_filter($allowt);
	$denyt = array_filter($denyt);
        // make list unique
        $allowu = array_unique($allowt);
        $denyu = array_unique($denyt);
	// sort list
	sort($allowu);
	sort($denyu);
        // convert back to a string
        $params['allowlist'] = implode(PHP_EOL, $allowu);
        $params['denylist'] = implode(PHP_EOL, $denyu);
        // Prepare the configuration object.
        $object = new \OMV\Config\ConfigObject('conf.service.safecmd.cmd');
        $object->setAssoc($params);
        // Set the configuration object.
        $isNew = $object->isNew();
        $db = \OMV\Config\Database::getInstance();
        if (TRUE === $isNew) {
            // Check uniqueness - name, command
            $db->assertIsUnique($object, 'name');
            $db->assertIsUnique($object, 'command');
        }
        $db->set($object);
        // Return the configuration object.
        return $object->getAssoc();
    }

    public function deleteCmd($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        // Delete the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get('conf.service.safecmd.cmd', $params['uuid']);
        $db->delete($object);
        // Return the deleted configuration object.
        return $object->getAssoc();
    }

    public function doCmd($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.safecmd.docmd');
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get('conf.service.safecmd.cmd', $params['uuid']);
	$command = $object->get('command');
        // Create the background process.
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
          use ($command) {
            $cmd = new \OMV\System\Process($command);
            $cmd->setRedirect2to1();
            $cmdLine = $cmd->getCommandLine();
            if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename))
                throw new \OMV\ExecException($cmdLine, $output);
            return $output;
        });
    }

    // User functions

    public function getUserCmdList($params, $context)
    {
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->get('conf.service.safecmd.cmd');
        // Add additional command information
        $objectsAssoc = [];
	$repl = [',',':', PHP_EOL];
	$br = '<br />';
        $user = $context['username'];
        foreach ($objects as $objectk => &$objectv) {
            $deny = explode(PHP_EOL, $objectv->get('denylist'));
	    if (in_array($user, $deny, false)) {
                continue;
            }
            $allow = explode(PHP_EOL, $objectv->get('allowlist'));
	    if (strlen($objectv->get('allowlist')) == 0 || in_array($user, $allow, false)) {
                $objectsAssoc[] = $objectv->getAssoc();
            }
        }
        // Filter the result.
        return $this->applyFilter($objectsAssoc, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function doCmdUser($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_EVERYONE]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.safecmd.docmd');
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get('conf.service.safecmd.cmd', $params['uuid']);
        $command = $object->get('command');
        $cmd = new \OMV\System\Process($command);
        // log run
        $logfile = '/var/log/omv-safecmd.log';
        $user = $context['username'];
        $dt = date('Y-m-d H:i:s');
        $logline = sprintf('%s%sRun by %s at %s%s', PHP_EOL, PHP_EOL, $user, $dt, PHP_EOL);
        file_put_contents($logfile, $logline, FILE_APPEND);
        $logline = sprintf('Command :: %s%s', $command, PHP_EOL);
        file_put_contents($logfile, $logline, FILE_APPEND);
        // log command output to log file if enabled
        if ($object->get('log')) {
            $cmd->setRedirect2to1(true);
        }
        // execute command
        $cmd->execute($output, $exitStatus);
        if ($object->get('log')) {
	    array_push($output, PHP_EOL);
	    file_put_contents($logfile, implode(PHP_EOL, $output), FILE_APPEND);
        }
    }
}
