# $Thus: source/cvs/Fakeroot-Stat/Makefile.PL,v 1.9.96.3 2007/11/29 15:05:56 benhaman-repoman Exp $

use ExtUtils::MakeMaker;

WriteMakefile(
	'NAME'		=> 'Fakeroot::Stat',
	'VERSION_FROM'	=> 'lib/Fakeroot/Stat.pm',
	'PREREQ_PM'	=> {
		'Test' => '0',
		},
	($] >= 5.005 ? (
		'ABSTRACT_FROM'	=> 'lib/Fakeroot/Stat.pm',
		'AUTHOR'	=> 'SCM Team at THUS plc <scm@thus.net>')
	: ()),
	'LIBS'	=> ["-lfakeroot"]
);
