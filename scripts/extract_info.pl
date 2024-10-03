#!/usr/bin/perl

use strict;
use warnings;
use Text::CSV;

# Create a new Text::CSV object
my $csv = Text::CSV->new({binary => 1, eol => "\n"});

# Open the input file
open my $input_file, "<", $ARGV[0] or die "Could not open input file: $!\n";

# Open the output CSV files
open my $localShrInfo, ">>", $ARGV[1] or die "Could not opent output file localShrInfo: $!\n";
open my $globalShrInfo, ">>", $ARGV[2] or die "Could not opent output file globalShrInfo: $!\n";

# Variables for totals
my ($total_local_received_cls, $total_local_shared_cls, $total_local_received_duplicas) = (0,0,0);
my ($total_global_received_cls, $total_global_shared_cls, $total_global_received_duplicas, $total_global_shared_duplicas_avoided, $total_global_message_sent) = (0,0,0,0,0);

while(<$input_file>) {

    my ($localID, $local_received_cls, $local_shared_cls, $local_received_duplicas) = /c Local Sharer\[(\d+-\d+)]:receivedCls (\d+),sharedCls (\d+),receivedDuplicas (\d+)/;
    my ($globalID, $global_received_cls, $global_shared_cls, $global_received_duplicas, $global_shared_duplicas_avoided, $global_message_sent) = /c Global Sharer\[(\d+-\d+)]:receivedCls (\d+),sharedCls (\d+),receivedDuplicas (\d+),sharedDuplicasAvoided (\d+),messagesSent (\d+)/;

    if(defined $localID){
        $total_local_received_cls += $local_received_cls;
        $total_local_shared_cls += $local_shared_cls;
        $total_local_received_duplicas += $local_received_duplicas; 
        $csv->print($localShrInfo, [$localID, $local_received_cls, $local_shared_cls, $local_received_duplicas]);
    }
    
    if(defined $globalID){
        $total_global_received_cls += $global_received_cls;
        $total_global_shared_cls += $global_shared_cls;
        $total_global_received_duplicas += $global_received_duplicas;
        $total_global_shared_duplicas_avoided += $global_shared_duplicas_avoided;
        $total_global_message_sent += $global_message_sent;
        $csv->print($globalShrInfo, [$globalID, $global_received_cls, $global_shared_cls, $global_received_duplicas, $global_shared_duplicas_avoided, $global_message_sent]);
    }
}

# Write totals
$csv->print($localShrInfo, [$ARGV[3]." Totals", $total_local_received_cls, $total_local_shared_cls, $total_local_received_duplicas]);
$csv->print($globalShrInfo, [$ARGV[3]." Totals", $total_global_received_cls, $total_global_shared_cls, $total_global_received_duplicas, $total_global_shared_duplicas_avoided, $total_global_message_sent]);

# Close the files
close $input_file or die "Could not close input file: $!\n";
close $localShrInfo or die "Could not close output file localShrInfo: $!\n";
close $globalShrInfo or die "Could not close output file globalShrInfo: $!\n";